/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.okular 2.0
import org.kde.kirigami 2.17 as Kirigami

Item {
    width: parent.width
    height: parent.height
    readonly property PageItem pageItem: page
    property alias document: page.document
    property alias pageNumber: page.pageNumber
    implicitWidth: page.implicitWidth
    implicitHeight: page.implicitHeight
    readonly property real pageRatio: page.implicitWidth / page.implicitHeight
    readonly property real scaleFactor: page.width / page.implicitWidth

    PageItem {
        id: page
        property bool sameOrientation: parent.width / parent.height > pageRatio
        anchors.centerIn: parent
        width: sameOrientation ? parent.height * pageRatio : parent.width
        height: !sameOrientation ? parent.width / pageRatio : parent.height
        document: null
    }

    Rectangle {
        id: backgroundRectangle
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: page.left
            right: page.right
            topMargin: -Kirigami.Units.gridUnit
            bottomMargin: -Kirigami.Units.gridUnit
        }
        z: -1
        color: "white"

        Rectangle {
            width: Kirigami.Units.gridUnit
            anchors {
                right: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: "transparent"
                }
                GradientStop {
                    position: 0.7
                    color: Qt.rgba(0, 0, 0, 0.08)
                }
                GradientStop {
                    position: 1.0
                    color: Qt.rgba(0, 0, 0, 0.2)
                }
            }
        }

        Rectangle {
            width: Kirigami.Units.gridUnit
            anchors {
                left: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: Qt.rgba(0, 0, 0, 0.2)
                }
                GradientStop {
                    position: 0.3
                    color: Qt.rgba(0, 0, 0, 0.08)
                }
                GradientStop {
                    position: 1.0
                    color: "transparent"
                }
            }
        }
    }
}
