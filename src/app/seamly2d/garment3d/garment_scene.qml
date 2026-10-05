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

// The 3D View dock's scene. All logic lives in C++ (GarmentViewWidget, GarmentSceneModel); this file only draws the
// pieces it is given, moves the camera and reports clicks. Units are cm, y points up.

import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Rectangle {
    id: root

    required property var sceneModel
    required property string emptyText
    required property string hintText
    required property color backgroundColor
    required property color textColor
    required property color highlightColor

    color: backgroundColor

    // Looks at all pieces straight on, from just far enough away to see them all.
    function frameAll() {
        const aspect = view.width / Math.max(view.height, 1)
        const half_vertical = camera.fieldOfView * Math.PI / 360
        const half_horizontal = Math.atan(Math.tan(half_vertical) * aspect)
        const half_view = Math.min(half_vertical, half_horizontal)
        const radius = Math.max(root.sceneModel.sceneRadius, 10)

        orbit_origin.position = root.sceneModel.sceneCenter
        orbit_origin.eulerRotation = Qt.vector3d(0, 0, 0)
        camera.position = Qt.vector3d(0, 0, radius / Math.sin(half_view) * 1.05)
    }

    Connections {
        target: root.sceneModel
        function onFramingRequested() {
            root.frameAll()
        }
    }

    View3D {
        id: view
        anchors.fill: parent

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        Node {
            id: orbit_origin

            PerspectiveCamera {
                id: camera
                z: 250
            }
        }

        // Key light from the front, fill light from behind so the back of a turned piece isn't black.
        DirectionalLight {
            eulerRotation: Qt.vector3d(-20, -25, 0)
            brightness: 1.0
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(20, 155, 0)
            brightness: 0.6
        }

        // The avatar, fitted to the pattern's measurements, in a plain grey like a dress form.
        Model {
            visible: root.sceneModel.hasAvatar
            geometry: root.sceneModel.avatarGeometry

            materials: PrincipledMaterial {
                baseColor: "#b9b4ad"
                roughness: 0.7
                metalness: 0.0
            }
        }

        // The pieces, flat on a board; with an avatar the board stands behind it.
        Node {
            position: root.sceneModel.boardOffset

            Repeater3D {
                model: root.sceneModel

                // Each piece sits a little in front of the one before, so pieces that overlap in the piece scene
                // don't flicker where they overlap.
                delegate: Node {
                    id: piece_node

                    required property int index
                    required property int pieceId
                    required property color pieceColor
                    required property Geometry pieceGeometry
                    required property Geometry pieceOutline
                    required property bool selected

                    z: index * 0.05

                    Model {
                        readonly property int pieceId: piece_node.pieceId

                        geometry: piece_node.pieceGeometry
                        pickable: true

                        // While a piece is selected the others step back, so the selection reads whatever the
                        // colors are.
                        materials: PrincipledMaterial {
                            baseColor: piece_node.selected
                                       ? Qt.tint(piece_node.pieceColor, Qt.rgba(root.highlightColor.r,
                                                                                root.highlightColor.g,
                                                                                root.highlightColor.b, 0.35))
                                       : root.sceneModel.selectedPiece !== 0 ? Qt.darker(piece_node.pieceColor, 1.8)
                                                                             : piece_node.pieceColor
                            roughness: 0.85
                            metalness: 0.0
                            cullMode: Material.NoCulling
                        }
                    }

                    // The seam line, just in front of the fabric, so same colored pieces can be told apart.
                    Model {
                        z: 0.02
                        geometry: piece_node.pieceOutline

                        materials: PrincipledMaterial {
                            lighting: PrincipledMaterial.NoLighting
                            baseColor: Qt.tint(piece_node.pieceColor, piece_node.pieceColor.hslLightness > 0.5
                                                                      ? "#80000000" : "#80ffffff")
                        }
                    }
                }
            }
        }
    }

    OrbitCameraController {
        anchors.fill: parent
        origin: orbit_origin
        camera: camera

        TapHandler {
            onTapped: (event_point) => {
                const result = view.pick(event_point.position.x, event_point.position.y)
                const hit = result.objectHit
                root.sceneModel.pickPiece(hit && hit.pieceId !== undefined ? hit.pieceId : 0)
            }
            onDoubleTapped: root.frameAll()
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
        text: root.hintText
        color: root.textColor
        opacity: 0.6
        font.pointSize: 8
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
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
