//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_scene.qml
//  @author Julius
//  @date   5 Oct, 2026
//
//  @copyright
//  Copyright (C)  2026 Seamly, LLC
//  https://github.com/fashionfreedom/seamly2d
//
//  @brief
//  Seamly2D is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Seamly2D is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Seamly2D. If not, see <http://www.gnu.org/licenses/>.
//---------------------------------------------------------------------------------------------------------------------

// The 3D View dock's scene. All logic lives in C++ (GarmentViewWidget, GarmentSceneModel, SeamEditor, StitchEditor);
// this file only draws what it is given, moves the camera and reports where the mouse is. Units are cm, y points up.

import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Rectangle {
    id: root

    required property var sceneModel
    required property var seamEditor
    required property var stitchEditor
    required property string emptyText
    required property string hintText
    required property color backgroundColor
    required property color textColor
    required property color highlightColor

    color: backgroundColor

    // How close, in pixels, the mouse has to come to a seam line to sew it or to pick a seam.
    readonly property real pickDistance: 8

    // How close, in pixels, the mouse has to come to an arrangement point to put a piece there.
    readonly property real snapDistance: 12

    // While arranging with a piece picked, the avatar's arrangement points show, those facing the camera.
    readonly property bool pointsShown: root.sceneModel.arranging && root.sceneModel.selectedPiece !== 0
                                        && root.sceneModel.hasAvatar && !root.capturing

    // The arrangement point under the mouse, -1 for none.
    property int pointUnderMouse: -1

    onPointsShownChanged: {
        if (!root.pointsShown) {
            root.pointUnderMouse = -1
        }
    }

    // While arranging, the picked piece on the avatar has a gizmo at its middle, as CLO's: arrows to move it around its
    // part of the body (red), up or down it (green) and out from it (blue), and rings to rotate it (blue, around the
    // outward arrow), lean it (red) and swing it (green). Its parts are numbered as GarmentSceneModel::GizmoPart.
    readonly property bool gizmoShown: root.sceneModel.arranging && root.sceneModel.gizmo.origin !== undefined
                                       && !root.capturing
    // In pixels: 70 long arrows, shorter in a small view, so the gizmo doesn't cover the whole garment there.
    readonly property real gizmoArrow: Math.max(36, Math.min(70, 0.2 * Math.min(root.width, root.height)))
    readonly property real gizmoRing: root.gizmoArrow * 0.65
    readonly property real gizmoGap: 12    // the middle is left to an arrangement point there
    readonly property real pointRadius: 6  // an arrangement point under the mouse this close takes it, not the gizmo
    readonly property real ringFacing: 0.3 // a ring seen more edge-on than this shows as a line, so it is left out
    readonly property var gizmoColors: ["#e0453a", "#3db24a", "#2f7fe0", "#2f7fe0", "#e0453a", "#3db24a"]

    // The part of the gizmo under the mouse, or held, -1 for none.
    property int gizmoUnderMouse: -1

    // Set while a part of the gizmo is held, which holds the camera still; what the gizmo was like when taken hold of.
    property bool draggingGizmo: false
    property var gizmoHeld: null

    onGizmoShownChanged: {
        if (!root.gizmoShown) {
            root.gizmoUnderMouse = -1
        }
    }

    // Where the ray through a point of the view meets the board of pieces, in the board's coordinates, or undefined
    // if it misses the board.
    function boardPoint(x, y) {
        const near = view.mapTo3DScene(Qt.vector3d(x, y, 0))
        const far = view.mapTo3DScene(Qt.vector3d(x, y, 100))
        const board = root.sceneModel.boardOffset
        const depth = far.z - near.z
        if (Math.abs(depth) < 1e-9) {
            return undefined
        }
        const t = (board.z - near.z) / depth
        if (t < 0) {
            return undefined
        }
        return Qt.vector2d(near.x + t * (far.x - near.x) - board.x, near.y + t * (far.y - near.y) - board.y)
    }

    // How far pickDistance pixels reach on the board next to a point of the view, in cm.
    function boardTolerance(x, y, point) {
        const beside = root.boardPoint(x + root.pickDistance, y)
        return beside === undefined ? 1 : beside.minus(point).length()
    }

    // The piece under a point of the view, on the board or on the avatar: its id, where the point is in the flat
    // piece, and how far pickDistance pixels reach there, in cm. Undefined over the avatar or nothing at all.
    function pieceSpot(x, y) {
        const result = view.pick(x, y)
        const target = result.objectHit
        if (!target || target.pieceId === undefined) {
            return undefined
        }
        const flat = root.sceneModel.restPoint(target.pieceId, result.position)
        if (flat === undefined) {
            return undefined
        }
        const across = 2 * result.distance * Math.tan(camera.fieldOfView * Math.PI / 360) / Math.max(view.height, 1)
        return { piece: target.pieceId, x: flat.x, y: flat.y, tolerance: root.pickDistance * across }
    }

    // Set while framing waits for the view to get a size, which its window only gives it after the scene has loaded.
    property bool framePending: false

    // Set while a placed piece is dragged around the avatar, which holds the camera still.
    property bool draggingPiece: false

    // Set while the cloth, or a pin, is pulled with the mouse while draping, which holds the camera still too.
    property bool pulling: false

    // Set while a snapshot is taken, which leaves out the hints over the scene.
    property bool capturing: false

    // How far from the eye the mouse took hold of the dragged piece, in cm.
    property real dragDistance: 0

    // Where the mouse is on the avatar, in scene coordinates; off the avatar, as far along the ray through the mouse as
    // where it took hold of the dragged piece.
    function dragPoint(x, y) {
        const results = view.pickAll(x, y)
        for (let i = 0; i < results.length; ++i) {
            if (results[i].objectHit && results[i].objectHit.isAvatar === true) {
                return results[i].scenePosition
            }
        }
        return root.rayPoint(x, y)
    }

    // The pin within pickDistance pixels of a point of the view, the nearest; -1 for none.
    function pinNear(x, y) {
        const pins = root.sceneModel.pins
        let found = -1
        let nearest = root.pickDistance
        for (let i = 0; i < pins.length; ++i) {
            const at = view.mapFrom3DScene(pins[i])
            const apart = Math.hypot(at.x - x, at.y - y)
            if (apart <= nearest) {
                nearest = apart
                found = i
            }
        }
        return found
    }

    // Whether an arrangement point faces the camera, and so shows.
    function facesCamera(point) {
        return point.normal.dotProduct(camera.scenePosition.minus(point.position)) > 0
    }

    // The arrangement point shown within reach of a point of the view, snapDistance pixels unless said otherwise, the
    // nearest; -1 for none.
    function arrangementPointNear(x, y, reach) {
        let found = -1
        if (root.pointsShown) {
            const points = root.sceneModel.arrangementPoints
            let nearest = reach === undefined ? root.snapDistance : reach
            for (let i = 0; i < points.length; ++i) {
                const at = view.mapFrom3DScene(points[i].position)
                const apart = Math.hypot(at.x - x, at.y - y)
                if (apart <= nearest && root.facesCamera(points[i])) {
                    nearest = apart
                    found = i
                }
            }
        }
        return found
    }

    // While arranging, shows where the picked piece would go: at the arrangement point under the mouse, or where the
    // mouse is on the avatar.
    function previewArrangement(x, y) {
        root.pointUnderMouse = root.arrangementPointNear(x, y)
        const result = view.pick(x, y)
        if (root.pointUnderMouse >= 0) {
            root.sceneModel.previewAtPoint(root.pointUnderMouse)
        } else if (result.objectHit && result.objectHit.isAvatar === true && !root.draggingPiece) {
            root.sceneModel.previewAt(result.scenePosition.x, result.scenePosition.y, result.scenePosition.z)
        } else {
            root.sceneModel.leaveAvatar()
        }
    }

    // Where the gizmo's arrows and rings are in the view: its middle, the tips of its arrows, and points around its
    // rings, and which rings show. Each ring turns its first direction towards its second: rotating up towards across,
    // clockwise as seen from outside; leaning up towards out; swinging across towards out. Its way is 1 if that turns
    // the view's angles up.
    function gizmoShape() {
        const gizmo = root.sceneModel.gizmo
        const center = view.mapFrom3DScene(gizmo.origin)
        const towards_eye = camera.scenePosition.minus(gizmo.origin).normalized()
        const distance = camera.scenePosition.minus(gizmo.origin).length()
        const per_pixel = 2 * distance * Math.tan(camera.fieldOfView * Math.PI / 360) / Math.max(view.height, 1)
        const tips = [gizmo.across, gizmo.up, gizmo.out].map(
            (axis) => view.mapFrom3DScene(gizmo.origin.plus(axis.times(root.gizmoArrow * per_pixel))))
        const rings = []
        const ways = []
        const shown = []
        for (const turn of [[gizmo.up, gizmo.across], [gizmo.up, gizmo.out], [gizmo.across, gizmo.out]]) {
            shown.push(Math.abs(turn[0].crossProduct(turn[1]).dotProduct(towards_eye)) >= root.ringFacing)
            const points = []
            const radius = root.gizmoRing * per_pixel
            for (let i = 0; i <= 48; ++i) {
                const t = 2 * Math.PI * i / 48
                points.push(view.mapFrom3DScene(gizmo.origin.plus(turn[0].times(Math.cos(t) * radius))
                                                .plus(turn[1].times(Math.sin(t) * radius))))
            }
            const first = points[0]
            const second = points[12]
            const across = (first.x - center.x) * (second.y - center.y) - (first.y - center.y) * (second.x - center.x)
            rings.push(points)
            ways.push(across < 0 ? -1 : 1)
        }
        return { center: center, tips: tips, rings: rings, ways: ways, shown: shown, perPixel: per_pixel }
    }

    // Where an arrow of the gizmo starts, clear of its middle.
    function gizmoTail(shape, part) {
        const tip = shape.tips[part]
        const length = Math.hypot(tip.x - shape.center.x, tip.y - shape.center.y)
        const share = length > root.gizmoGap ? root.gizmoGap / length : 1
        return Qt.point(shape.center.x + (tip.x - shape.center.x) * share,
                        shape.center.y + (tip.y - shape.center.y) * share)
    }

    // How far a point of the view is from a line between two others, in pixels.
    function distanceToSegment(x, y, from, to) {
        const dx = to.x - from.x
        const dy = to.y - from.y
        const length_squared = dx * dx + dy * dy
        const along = length_squared > 0 ? ((x - from.x) * dx + (y - from.y) * dy) / length_squared : 0
        const t = Math.max(0, Math.min(1, along))
        return Math.hypot(x - from.x - t * dx, y - from.y - t * dy)
    }

    // The part of the gizmo within pickDistance pixels of a point of the view, an arrow before a ring, the nearest; -1
    // for none, and where an arrangement point is right under the mouse.
    function gizmoPartAt(x, y) {
        let found = -1
        if (root.gizmoShown && root.arrangementPointNear(x, y, root.pointRadius) < 0) {
            const shape = root.gizmoShape()
            let nearest = root.pickDistance
            for (let part = 0; part < 3; ++part) {
                const tip = shape.tips[part]
                const shown = Math.hypot(tip.x - shape.center.x, tip.y - shape.center.y) > root.gizmoGap
                const apart = root.distanceToSegment(x, y, root.gizmoTail(shape, part), tip)
                if (shown && apart <= nearest) {
                    nearest = apart
                    found = part
                }
            }
            const on_arrow = found >= 0
            for (let ring = 0; ring < 3 && !on_arrow; ++ring) {
                const points = shape.rings[ring]
                for (let i = 0; i + 1 < points.length && shape.shown[ring]; ++i) {
                    const apart = root.distanceToSegment(x, y, points[i], points[i + 1])
                    if (apart <= nearest) {
                        nearest = apart
                        found = 3 + ring
                    }
                }
            }
        }
        return found
    }

    // Takes hold of a part of the gizmo at a point of the view; says whether it did.
    function grabGizmo(part, x, y) {
        const shape = root.gizmoShape()
        root.gizmoHeld = { part: part, shape: shape, x: x, y: y,
                           angle: Math.atan2(y - shape.center.y, x - shape.center.x) * 180 / Math.PI, turned: 0 }
        root.gizmoUnderMouse = part
        return root.sceneModel.grabGizmo(part)
    }

    // The mouse moved on with a part of the gizmo held: how far along its arrow, in cm, or around its ring, in
    // degrees, as the gizmo was when taken hold of.
    function dragGizmo(x, y) {
        const held = root.gizmoHeld
        const shape = held.shape
        let amount = 0
        if (held.part < 3) {
            const tip = shape.tips[held.part]
            const dx = tip.x - shape.center.x
            const dy = tip.y - shape.center.y
            const length_squared = dx * dx + dy * dy
            if (length_squared > 1) {
                amount = ((x - held.x) * dx + (y - held.y) * dy) / length_squared * root.gizmoArrow * shape.perPixel
            }
        } else {
            const angle = Math.atan2(y - shape.center.y, x - shape.center.x) * 180 / Math.PI
            let step = angle - held.angle
            while (step > 180) {
                step -= 360
            }
            while (step <= -180) {
                step += 360
            }
            held.turned += step
            held.angle = angle
            amount = held.turned * shape.ways[held.part - 3]
        }
        root.sceneModel.dragGizmo(amount)
    }

    // The point along the ray through a point of the view as far from the eye as where the mouse took hold.
    function rayPoint(x, y) {
        const near = view.mapTo3DScene(Qt.vector3d(x, y, 0))
        const far = view.mapTo3DScene(Qt.vector3d(x, y, 100))
        return near.plus(far.minus(near).normalized().times(root.dragDistance))
    }

    // Looks at all pieces straight on, from just far enough away to see them all.
    function frameAll() {
        root.framePending = view.width < 1 || view.height < 1
        if (root.framePending) {
            return
        }

        const aspect = view.width / Math.max(view.height, 1)
        const half_vertical = camera.fieldOfView * Math.PI / 360
        const half_horizontal = Math.atan(Math.tan(half_vertical) * aspect)
        const half_view = Math.min(half_vertical, half_horizontal)
        const radius = Math.max(root.sceneModel.sceneRadius, 10)

        orbit_origin.position = root.sceneModel.sceneCenter
        orbit_origin.eulerRotation = Qt.vector3d(0, 0, 0)
        camera.position = Qt.vector3d(0, 0, radius / Math.sin(half_view) * 1.05)
    }

    // Looks at all pieces from a direction: turned up or down by the pitch and around by the yaw, in degrees.
    function viewFrom(pitch, yaw) {
        root.frameAll()
        orbit_origin.eulerRotation = Qt.vector3d(pitch, yaw, 0)
    }

    Connections {
        target: root.sceneModel
        function onFramingRequested() {
            root.frameAll()
        }
        function onViewRequested(pitch, yaw) {
            root.viewFrom(pitch, yaw)
        }
    }

    View3D {
        id: view
        anchors.fill: parent
        // Rendering would find the camera by itself, mapTo3DScene() needs to be told.
        camera: camera

        onWidthChanged: {
            if (root.framePending) {
                root.frameAll()
            }
        }
        onHeightChanged: {
            if (root.framePending) {
                root.frameAll()
            }
        }

        // Ambient occlusion darkens the cloth in its folds and where it lies close to the body, as daylight does, soft and
        // wide enough not to pick out the triangles the cloth is made of; filmic tone mapping keeps light cloth from
        // washing out to white.
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            aoEnabled: true
            aoStrength: 45
            aoDistance: 12
            aoSoftness: 100
            aoSampleRate: 3
            tonemapMode: SceneEnvironment.TonemapModeFilmic
        }

        Node {
            id: orbit_origin

            PerspectiveCamera {
                id: camera
                z: 250
            }
        }

        // Key light from above the front, casting soft shadows, with a little ambient light so nothing in shadow goes
        // black; fill light from behind so the back of a turned piece isn't dark.
        DirectionalLight {
            eulerRotation: Qt.vector3d(-40, -25, 0)
            brightness: 1.5
            castsShadow: true
            shadowMapQuality: Light.ShadowMapQualityVeryHigh
            softShadowQuality: Light.PCF16
            pcfFactor: 3
            shadowFactor: 45
            shadowBias: 0.3
            shadowMapFar: Math.max(root.sceneModel.sceneRadius * 6, 500)
            ambientColor: Qt.rgba(0.3, 0.3, 0.3, 1)
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(20, 155, 0)
            brightness: 0.6
        }

        // The floor the avatar stands on, a shade off the background, for its shadow and the garment's to fall on.
        Model {
            visible: root.sceneModel.hasAvatar
            source: "#Cylinder"
            position: root.sceneModel.avatarFloor.minus(Qt.vector3d(0, 0.1, 0))
            scale: Qt.vector3d(root.sceneModel.avatarReach / 50, 0.002, root.sceneModel.avatarReach / 50)
            castsShadows: false
            pickable: false

            materials: PrincipledMaterial {
                baseColor: Qt.darker(root.backgroundColor, root.backgroundColor.hslLightness > 0.5 ? 1.08 : 0.8)
                roughness: 1.0
                metalness: 0.0
            }
        }

        // The avatar, fitted to the pattern's measurements, in a plain grey like a dress form. While sewing it fades,
        // so the board behind it can be seen; while arranging it can be clicked to put pieces on, and while
        // topstitching it keeps clicks off the board behind it.
        Model {
            readonly property bool isAvatar: true

            visible: root.sceneModel.hasAvatar && (root.sceneModel.avatarShown || root.sceneModel.arranging)
            opacity: root.seamEditor.sewing ? 0.25 : 1.0
            pickable: root.sceneModel.arranging || root.stitchEditor.stitching
            geometry: root.sceneModel.avatarGeometry

            materials: PrincipledMaterial {
                baseColor: "#b9b4ad"
                roughness: 0.7
                metalness: 0.0
            }
        }

        // The seams on the pieces on the avatar, in scene coordinates as the pieces there: tubes along their sides, and
        // lines between the places that meet while the pieces hang apart. While sewing they always show, with the
        // segments being sewn over them.
        Node {
            visible: root.seamEditor.garmentSeamsShown || root.seamEditor.sewing

            PrincipledMaterial {
                id: garment_seam_material
                lighting: PrincipledMaterial.NoLighting
                vertexColorsEnabled: true
                cullMode: Material.NoCulling
            }

            Model {
                geometry: root.seamEditor.garmentSeams
                materials: garment_seam_material
                castsShadows: false
            }

            Model {
                geometry: root.seamEditor.garmentLines
                materials: garment_seam_material
                castsShadows: false
            }

            Model {
                geometry: root.seamEditor.garmentPreview
                materials: garment_seam_material
                castsShadows: false
            }

            Model {
                geometry: root.seamEditor.garmentPreviewLines
                materials: garment_seam_material
                castsShadows: false
            }
        }

        // Checks woven along the grain, the wider stripe along it, repeating every checkRepeat cm; the piece's color
        // tints them. They follow the pieces' second texture coordinates, which run across and along the grain.
        Texture {
            id: checks_texture
            source: "textures/checks.png"
            indexUV: 1
            scaleU: 1.0 / root.sceneModel.checkRepeat
            scaleV: 1.0 / root.sceneModel.checkRepeat
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
            generateMipmaps: true
            mipFilter: Texture.Linear
        }

        // The pins holding the cloth, small heads in the highlight color, as large on the screen however far off.
        Repeater3D {
            model: root.sceneModel.pins

            delegate: Model {
                required property var modelData

                source: "#Sphere"
                position: modelData
                scale: {
                    const size = camera.scenePosition.minus(modelData).length() * 0.00012
                    return Qt.vector3d(size, size, size)
                }
                castsShadows: false

                materials: PrincipledMaterial {
                    baseColor: root.highlightColor
                    roughness: 0.35
                    metalness: 0.0
                }
            }
        }

        // Where the picked piece would go while arranging, see-through, in scene coordinates as placed pieces.
        Model {
            visible: root.sceneModel.previewShown && root.sceneModel.arranging
            geometry: root.sceneModel.previewGeometry
            opacity: 0.5
            castsShadows: false
            pickable: false

            materials: PrincipledMaterial {
                baseColor: root.highlightColor
                roughness: 0.85
                metalness: 0.0
                cullMode: Material.NoCulling
            }
        }

        // The pieces, flat on a board; with an avatar the board stands behind it.
        Node {
            position: root.sceneModel.boardOffset

            Repeater3D {
                model: root.sceneModel

                // Each piece on the board sits a little in front of the one before, so pieces that overlap in the
                // piece scene don't flicker where they overlap. Placed pieces come in scene coordinates, so they
                // undo the board's move.
                delegate: Node {
                    id: piece_node

                    required property int index
                    required property int pieceId
                    required property color pieceColor
                    required property Geometry pieceGeometry
                    required property Geometry pieceOutline
                    required property Geometry pieceStitches
                    required property Geometry pieceStitchPreview
                    required property color pieceThreadColor
                    required property TextureData pieceTexture
                    required property size pieceTextureSize
                    required property Geometry pieceEdges
                    required property bool pieceShown
                    required property bool selected
                    required property bool placed

                    position: placed ? root.sceneModel.boardOffset.times(-1) : Qt.vector3d(0, 0, index * 0.05)
                    visible: pieceShown

                    // With a fit map shown, the vertex colors take the place of the piece's own; with an image of
                    // its fabric, the image does.
                    readonly property color ownColor: root.sceneModel.fitMapShown || pieceTexture ? "white"
                                                                                                  : pieceColor

                    // While a piece is selected the others step back, so the selection reads whatever the colors are.
                    readonly property color clothColor: selected
                                                        ? Qt.tint(ownColor, Qt.rgba(root.highlightColor.r,
                                                                                    root.highlightColor.g,
                                                                                    root.highlightColor.b, 0.35))
                                                        : root.sceneModel.selectedPiece !== 0 ? Qt.darker(ownColor, 1.8)
                                                                                              : ownColor

                    // The image of the piece's fabric, in place of the checks: as wide and as long as the cloth it
                    // shows, repeating across the grain and along it, its top towards where the grainline points.
                    Texture {
                        id: fabric_texture
                        textureData: piece_node.pieceTexture
                        indexUV: 1
                        flipV: true
                        scaleU: 1.0 / Math.max(piece_node.pieceTextureSize.width, 0.1)
                        scaleV: 1.0 / Math.max(piece_node.pieceTextureSize.height, 0.1)
                        tilingModeHorizontal: Texture.Repeat
                        tilingModeVertical: Texture.Repeat
                        generateMipmaps: true
                        mipFilter: Texture.Linear
                    }

                    Model {
                        readonly property int pieceId: piece_node.pieceId

                        geometry: piece_node.pieceGeometry
                        pickable: true

                        materials: PrincipledMaterial {
                            baseColor: piece_node.clothColor
                            vertexColorsEnabled: root.sceneModel.fitMapShown
                            baseColorMap: root.sceneModel.fitMapShown ? null
                                          : piece_node.pieceTexture ? fabric_texture
                                          : root.sceneModel.checksShown ? checks_texture : null
                            roughness: 0.85
                            metalness: 0.0
                            // The cloth has a front face and a back face, each lit as it faces.
                            cullMode: Material.BackFaceCulling
                        }
                    }

                    // The topstitching, on both faces of the cloth.
                    Model {
                        visible: piece_node.pieceStitches.stitchCount > 0
                        geometry: piece_node.pieceStitches

                        materials: PrincipledMaterial {
                            baseColor: root.sceneModel.selectedPiece !== 0 && !piece_node.selected
                                       ? Qt.darker(piece_node.pieceThreadColor, 1.8) : piece_node.pieceThreadColor
                            roughness: 0.6
                            metalness: 0.0
                        }
                    }

                    // The topstitching the edge under the mouse would get, over any already there.
                    Model {
                        visible: piece_node.pieceStitchPreview.stitchCount > 0
                        geometry: piece_node.pieceStitchPreview
                        castsShadows: false

                        materials: PrincipledMaterial {
                            lighting: PrincipledMaterial.NoLighting
                            baseColor: root.highlightColor
                        }
                    }

                    // The triangles the cloth is made of, on both its faces, when asked for.
                    Model {
                        visible: root.sceneModel.meshShown
                        geometry: piece_node.pieceEdges
                        castsShadows: false
                        pickable: false

                        materials: PrincipledMaterial {
                            lighting: PrincipledMaterial.NoLighting
                            baseColor: piece_node.pieceColor.hslLightness > 0.5 ? "#99303030" : "#99f0f0f0"
                        }
                    }

                    // The seam line, just in front of the fabric, so same colored pieces can be told apart.
                    Model {
                        z: 0.02
                        geometry: piece_node.pieceOutline
                        castsShadows: false

                        materials: PrincipledMaterial {
                            lighting: PrincipledMaterial.NoLighting
                            baseColor: Qt.tint(piece_node.pieceColor, piece_node.pieceColor.hslLightness > 0.5
                                                                      ? "#80000000" : "#80ffffff")
                        }
                    }
                }
            }

            // The seams, in front of all pieces so they always show, and in front of them what is being sewn.
            Node {
                z: root.sceneModel.pieceCount * 0.05 + 0.05

                PrincipledMaterial {
                    id: seam_material
                    lighting: PrincipledMaterial.NoLighting
                    vertexColorsEnabled: true
                    cullMode: Material.NoCulling
                }

                Model {
                    geometry: root.seamEditor.seamBands
                    materials: seam_material
                    castsShadows: false
                }

                Model {
                    z: 0.01
                    geometry: root.seamEditor.seamLines
                    materials: seam_material
                    castsShadows: false
                }

                Model {
                    z: 0.02
                    geometry: root.seamEditor.previewBands
                    materials: seam_material
                    castsShadows: false
                }

                Model {
                    z: 0.03
                    geometry: root.seamEditor.previewLines
                    materials: seam_material
                    castsShadows: false
                }
            }
        }
    }

    // How much longer one side of each seam is than the other, at the seam, in the seam's color, when asked for.
    Repeater {
        model: root.seamEditor.lengthsShown ? root.seamEditor.lengthLabels : []

        Rectangle {
            required property var modelData

            // Where the seam is in the view; worked out again whenever the camera or the view moves.
            readonly property vector3d onView: {
                camera.scenePosition
                camera.sceneRotation
                view.width
                view.height
                return view.mapFrom3DScene(modelData.onBoard ? modelData.position.plus(root.sceneModel.boardOffset)
                                                             : modelData.position)
            }

            x: onView.x - width / 2
            y: onView.y - height / 2
            width: length_text.implicitWidth + 8
            height: length_text.implicitHeight + 2
            radius: 3
            color: modelData.color

            Text {
                id: length_text
                anchors.centerIn: parent
                text: parent.modelData.text
                color: parent.modelData.color.hslLightness > 0.55 ? "black" : "white"
                font.pointSize: 8
            }
        }
    }

    // The avatar's arrangement points while they show, as CLO's: small rings on the body, the one under the mouse
    // filled.
    Repeater {
        model: root.pointsShown ? root.sceneModel.arrangementPoints : []

        Rectangle {
            required property var modelData
            required property int index

            // Where the point is in the view, and whether it faces the camera; worked out again whenever the camera
            // or the view moves.
            readonly property vector3d onView: {
                camera.scenePosition
                camera.sceneRotation
                view.width
                view.height
                return view.mapFrom3DScene(modelData.position)
            }
            readonly property bool facing: {
                camera.scenePosition
                return root.facesCamera(modelData)
            }
            readonly property bool underMouse: root.pointUnderMouse === index

            visible: facing
            x: onView.x - width / 2
            y: onView.y - height / 2
            width: underMouse ? 12 : 8
            height: width
            radius: width / 2
            color: underMouse ? root.highlightColor : "white"
            border.color: root.highlightColor
            border.width: 2
        }
    }

    // The gizmo, drawn over the scene so the cloth never hides it; the part under the mouse, or held, in yellow and
    // thicker.
    Canvas {
        id: gizmo_canvas
        anchors.fill: parent
        visible: root.gizmoShown

        onPaint: {
            const context = getContext("2d")
            context.reset()
            if (!root.gizmoShown) {
                return
            }
            const shape = root.gizmoShape()
            context.lineCap = "round"
            context.lineJoin = "round"
            const stroke = (part, draw) => {
                const held = root.gizmoUnderMouse === part
                for (const pass of [0, 1]) {
                    context.beginPath()
                    draw()
                    context.lineWidth = (held ? 4 : 2) + (pass === 0 ? 2 : 0)
                    context.strokeStyle = pass === 0 ? "#80000000" : held ? "#ffd400" : root.gizmoColors[part]
                    context.stroke()
                }
            }
            for (let ring = 0; ring < 3; ++ring) {
                if (!shape.shown[ring]) {
                    continue
                }
                stroke(3 + ring, () => {
                    const points = shape.rings[ring]
                    context.moveTo(points[0].x, points[0].y)
                    for (let i = 1; i < points.length; ++i) {
                        context.lineTo(points[i].x, points[i].y)
                    }
                })
            }
            for (let part = 0; part < 3; ++part) {
                const tail = root.gizmoTail(shape, part)
                const tip = shape.tips[part]
                const length = Math.hypot(tip.x - tail.x, tip.y - tail.y)
                if (length < 1) {
                    continue
                }
                const ux = (tip.x - tail.x) / length
                const uy = (tip.y - tail.y) / length
                stroke(part, () => {
                    context.moveTo(tail.x, tail.y)
                    context.lineTo(tip.x, tip.y)
                    context.moveTo(tip.x - ux * 10 - uy * 5, tip.y - uy * 10 + ux * 5)
                    context.lineTo(tip.x, tip.y)
                    context.lineTo(tip.x - ux * 10 + uy * 5, tip.y - uy * 10 - ux * 5)
                })
            }
        }

        Connections {
            target: camera
            function onScenePositionChanged() {
                gizmo_canvas.requestPaint()
            }
            function onSceneRotationChanged() {
                gizmo_canvas.requestPaint()
            }
        }
        Connections {
            target: root.sceneModel
            function onGizmoChanged() {
                gizmo_canvas.requestPaint()
            }
        }
        Connections {
            target: root
            function onGizmoUnderMouseChanged() {
                gizmo_canvas.requestPaint()
            }
            function onGizmoShownChanged() {
                gizmo_canvas.requestPaint()
            }
        }
        onWidthChanged: gizmo_canvas.requestPaint()
        onHeightChanged: gizmo_canvas.requestPaint()
    }

    OrbitCameraController {
        anchors.fill: parent
        origin: orbit_origin
        camera: camera
        mouseEnabled: !root.draggingPiece && !root.pulling && !root.draggingGizmo

        // While arranging, a part of the picked piece's gizmo pressed on moves or turns the piece with the mouse until
        // it is let go; a placed piece pressed on elsewhere follows the mouse around the avatar, unless an arrangement
        // point shows over it there, which takes the click.
        PointHandler {
            id: piece_handler
            enabled: root.sceneModel.arranging
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.NoModifier

            onActiveChanged: {
                if (piece_handler.active) {
                    const x = piece_handler.point.position.x
                    const y = piece_handler.point.position.y
                    const part = root.gizmoPartAt(x, y)
                    if (part >= 0) {
                        root.draggingGizmo = root.grabGizmo(part, x, y)
                        return
                    }
                    const results = view.pickAll(x, y)
                    const piece = results.length > 0 ? results[0].objectHit : null
                    if (piece && piece.pieceId !== undefined && root.arrangementPointNear(x, y) < 0) {
                        let held = results[0].scenePosition
                        for (let i = 1; i < results.length; ++i) {
                            if (results[i].objectHit && results[i].objectHit.isAvatar === true) {
                                held = results[i].scenePosition
                                break
                            }
                        }
                        root.dragDistance = held.minus(view.mapTo3DScene(Qt.vector3d(x, y, 0))).length()
                        root.draggingPiece = root.sceneModel.grabPiece(piece.pieceId, held.x, held.y, held.z)
                    }
                } else if (root.draggingGizmo) {
                    root.draggingGizmo = false
                    root.gizmoUnderMouse = -1
                    root.sceneModel.dropGizmo()
                } else if (root.draggingPiece) {
                    root.draggingPiece = false
                    root.sceneModel.dropPiece()
                }
            }
            onPointChanged: {
                if (root.draggingGizmo && piece_handler.active) {
                    root.dragGizmo(piece_handler.point.position.x, piece_handler.point.position.y)
                } else if (root.draggingPiece && piece_handler.active) {
                    const at = root.dragPoint(piece_handler.point.position.x, piece_handler.point.position.y)
                    root.sceneModel.dragTo(at.x, at.y, at.z)
                }
            }
        }

        // While arranging, a click on the avatar places the selected piece there; while topstitching, a click near an
        // edge stitches it. Otherwise seams get the click first, and what they leave selects a piece.
        // While draping, cloth on the avatar pressed on is held by the mouse and pulled where it goes, at the distance
        // it was taken hold of; a pin pressed on moves with the mouse, the cloth with it.
        PointHandler {
            id: pull_handler
            enabled: root.sceneModel.simulating && !root.sceneModel.arranging && !root.seamEditor.sewing
                     && !root.stitchEditor.stitching
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.NoModifier

            onActiveChanged: {
                if (pull_handler.active) {
                    const x = pull_handler.point.position.x
                    const y = pull_handler.point.position.y
                    const pin = root.pinNear(x, y)
                    const eye = view.mapTo3DScene(Qt.vector3d(x, y, 0))
                    if (pin >= 0) {
                        root.pulling = root.sceneModel.pullPin(pin)
                        root.dragDistance = root.sceneModel.pins[pin].minus(eye).length()
                    } else {
                        const result = view.pick(x, y)
                        const target = result.objectHit
                        if (target && target.pieceId !== undefined) {
                            root.pulling = root.sceneModel.pullCloth(target.pieceId, result.scenePosition.x,
                                                                     result.scenePosition.y, result.scenePosition.z)
                            root.dragDistance = result.scenePosition.minus(eye).length()
                        }
                    }
                } else if (root.pulling) {
                    root.pulling = false
                    root.sceneModel.releasePull()
                }
            }
            onPointChanged: {
                if (root.pulling && pull_handler.active) {
                    const at = root.rayPoint(pull_handler.point.position.x, pull_handler.point.position.y)
                    root.sceneModel.pullTo(at.x, at.y, at.z)
                }
            }
        }

        // Shift+click pins the cloth on the avatar where it is clicked, or takes a pin clicked out.
        TapHandler {
            acceptedModifiers: Qt.ShiftModifier
            enabled: !root.sceneModel.arranging && !root.seamEditor.sewing && !root.stitchEditor.stitching

            onTapped: (event_point) => {
                const pin = root.pinNear(event_point.position.x, event_point.position.y)
                const result = view.pick(event_point.position.x, event_point.position.y)
                const target = result.objectHit
                if (pin >= 0) {
                    root.sceneModel.unpin(pin)
                } else if (target && target.pieceId !== undefined) {
                    root.sceneModel.pinCloth(target.pieceId, result.scenePosition.x, result.scenePosition.y,
                                             result.scenePosition.z)
                }
            }
        }

        TapHandler {
            acceptedModifiers: Qt.NoModifier

            onTapped: (event_point) => {
                const x = event_point.position.x
                const y = event_point.position.y
                if (root.stitchEditor.stitching) {
                    const spot = root.pieceSpot(x, y)
                    if (spot !== undefined) {
                        root.stitchEditor.click(spot.piece, spot.x, spot.y, spot.tolerance)
                    }
                    return
                }
                if (root.sceneModel.arranging) {
                    if (root.gizmoPartAt(x, y) >= 0) {
                        return
                    }
                    const point = root.arrangementPointNear(x, y)
                    if (point >= 0) {
                        root.sceneModel.placeAtPoint(point)
                        return
                    }
                    const result = view.pick(x, y)
                    const target = result.objectHit
                    if (target && target.isAvatar === true) {
                        root.sceneModel.placeAt(result.scenePosition.x, result.scenePosition.y,
                                                result.scenePosition.z)
                    } else {
                        root.sceneModel.pickPiece(target && target.pieceId !== undefined ? target.pieceId : 0)
                    }
                    return
                }

                // A piece on the avatar takes the click itself, the board the click on it.
                const spot = root.pieceSpot(x, y)
                const on_avatar = spot !== undefined && root.sceneModel.isPlaced(spot.piece)
                const point = root.boardPoint(x, y)
                const used = on_avatar ? root.seamEditor.clickPiece(spot.piece, spot.x, spot.y, spot.tolerance)
                                       : point !== undefined
                                         && root.seamEditor.click(point.x, point.y, root.boardTolerance(x, y, point))
                if (!used) {
                    const hit = view.pick(x, y).objectHit
                    root.sceneModel.pickPiece(hit && hit.pieceId !== undefined ? hit.pieceId : 0)
                }
            }
            onDoubleTapped: root.frameAll()
        }

        // A right-click on a piece on the avatar picks it and asks what to do with it: rotate it, turn it over, take
        // it off.
        TapHandler {
            acceptedButtons: Qt.RightButton
            enabled: !root.seamEditor.sewing && !root.stitchEditor.stitching

            onTapped: (event_point) => {
                const target = view.pick(event_point.position.x, event_point.position.y).objectHit
                if (target && target.pieceId !== undefined) {
                    root.sceneModel.showPieceMenu(target.pieceId, event_point.position.x, event_point.position.y)
                }
            }
        }

        HoverHandler {
            id: hover_handler
            cursorShape: root.seamEditor.sewing || root.stitchEditor.stitching ? Qt.CrossCursor
                         : root.pointUnderMouse >= 0 || root.gizmoUnderMouse >= 0 ? Qt.PointingHandCursor
                                                                                  : Qt.ArrowCursor

            onPointChanged: {
                const x = hover_handler.point.position.x
                const y = hover_handler.point.position.y
                if (root.sceneModel.arranging) {
                    if (!root.draggingGizmo) {
                        const part = root.gizmoPartAt(x, y)
                        if (part !== root.gizmoUnderMouse) {
                            root.gizmoUnderMouse = part
                            root.sceneModel.hoverGizmo(part)
                        }
                        if (part >= 0) {
                            root.pointUnderMouse = -1
                            return
                        }
                        root.previewArrangement(x, y)
                    }
                    return
                }
                if (root.stitchEditor.stitching) {
                    const spot = root.pieceSpot(x, y)
                    if (spot === undefined) {
                        root.stitchEditor.leave()
                    } else {
                        root.stitchEditor.hover(spot.piece, spot.x, spot.y, spot.tolerance)
                    }
                    return
                }
                if (!root.seamEditor.sewing) {
                    return
                }
                const spot = root.pieceSpot(x, y)
                const point = root.boardPoint(x, y)
                if (spot !== undefined && root.sceneModel.isPlaced(spot.piece)) {
                    root.seamEditor.hoverPiece(spot.piece, spot.x, spot.y, spot.tolerance)
                } else if (point === undefined) {
                    root.seamEditor.leave()
                } else {
                    root.seamEditor.hover(point.x, point.y, root.boardTolerance(x, y, point))
                }
            }
            onHoveredChanged: {
                if (!hover_handler.hovered) {
                    root.seamEditor.leave()
                    root.stitchEditor.leave()
                    if (root.sceneModel.arranging) {
                        root.pointUnderMouse = -1
                        root.sceneModel.leaveAvatar()
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 20
        visible: root.sceneModel.pieceCount === 0 && !root.sceneModel.hasAvatar
        text: root.emptyText
        color: root.textColor
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        visible: (root.sceneModel.pieceCount > 0 || root.sceneModel.hasAvatar) && !root.capturing
        readonly property string task: root.sceneModel.hint !== "" ? root.sceneModel.hint
                                       : root.stitchEditor.hint !== "" ? root.stitchEditor.hint
                                                                       : root.seamEditor.hint

        text: task !== "" ? task : root.hintText
        color: root.textColor
        opacity: task !== "" ? 1.0 : 0.6
        font.pointSize: 8
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    // How the fit map's colors read: the values its four colors stand for, the lowest at the bottom.
    Row {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 8
        spacing: 4
        visible: root.sceneModel.fitMapShown

        Item {
            id: fit_labels
            width: 48
            height: fit_bar.height

            Repeater {
                model: root.sceneModel.fitLabels

                Text {
                    required property int index
                    required property string modelData

                    anchors.right: parent.right
                    y: (fit_labels.height - height) * (1 - index / 3)
                    text: modelData
                    color: root.textColor
                    font.pointSize: 8
                }
            }
        }

        Rectangle {
            id: fit_bar
            width: 10
            height: 120
            gradient: Gradient {
                GradientStop { position: 0.0; color: root.sceneModel.fitColors[3] }
                GradientStop { position: 1 / 3; color: root.sceneModel.fitColors[2] }
                GradientStop { position: 2 / 3; color: root.sceneModel.fitColors[1] }
                GradientStop { position: 1.0; color: root.sceneModel.fitColors[0] }
            }
        }
    }

    // What the avatar couldn't match, if anything.
    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 6
        visible: root.sceneModel.avatarNote !== "" && !root.capturing
        text: root.sceneModel.avatarNote
        color: root.textColor
        font.pointSize: 8
        wrapMode: Text.WordWrap
    }

    Component.onCompleted: {
        if (root.sceneModel.pieceCount > 0 || root.sceneModel.hasAvatar) {
            root.frameAll()
        }
    }
}
