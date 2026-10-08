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

    OrbitCameraController {
        anchors.fill: parent
        origin: orbit_origin
        camera: camera
        mouseEnabled: !root.draggingPiece

        // While arranging, a placed piece pressed on follows the mouse around the avatar until it is let go.
        PointHandler {
            id: piece_handler
            enabled: root.sceneModel.arranging
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.NoModifier

            onActiveChanged: {
                if (piece_handler.active) {
                    const x = piece_handler.point.position.x
                    const y = piece_handler.point.position.y
                    const results = view.pickAll(x, y)
                    const piece = results.length > 0 ? results[0].objectHit : null
                    if (piece && piece.pieceId !== undefined) {
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
                } else if (root.draggingPiece) {
                    root.draggingPiece = false
                    root.sceneModel.dropPiece()
                }
            }
            onPointChanged: {
                if (root.draggingPiece && piece_handler.active) {
                    const at = root.dragPoint(piece_handler.point.position.x, piece_handler.point.position.y)
                    root.sceneModel.dragTo(at.x, at.y, at.z)
                }
            }
        }

        // While arranging, a click on the avatar places the selected piece there; while topstitching, a click near an
        // edge stitches it. Otherwise seams get the click first, and what they leave selects a piece.
        TapHandler {
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

        HoverHandler {
            id: hover_handler
            cursorShape: root.seamEditor.sewing || root.stitchEditor.stitching ? Qt.CrossCursor : Qt.ArrowCursor

            onPointChanged: {
                const x = hover_handler.point.position.x
                const y = hover_handler.point.position.y
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
        visible: root.sceneModel.pieceCount > 0 || root.sceneModel.hasAvatar
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
        visible: root.sceneModel.avatarNote !== ""
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
