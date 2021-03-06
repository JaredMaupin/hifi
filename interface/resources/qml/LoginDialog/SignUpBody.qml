//
//  SignUpBody.qml
//
//  Created by Stephen Birarda on 7 Dec 2016
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import Hifi 1.0
import QtQuick 2.7
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4 as OriginalStyles

import "../controls-uit"
import "../styles-uit"

Item {
    id: signupBody
    clip: true
    height: root.pane.height
    width: root.pane.width

    function signup() {
        mainTextContainer.visible = false
        toggleLoading(true)
        loginDialog.signup(emailField.text, usernameField.text, passwordField.text)
    }

    property bool keyboardEnabled: false
    property bool keyboardRaised: false
    property bool punctuationMode: false

    onKeyboardRaisedChanged: d.resize();

    QtObject {
        id: d
        readonly property int minWidth: 480
        readonly property int maxWidth: 1280
        readonly property int minHeight: 120
        readonly property int maxHeight: 720

        function resize() {
            var targetWidth = Math.max(titleWidth, form.contentWidth);
            var targetHeight =  hifi.dimensions.contentSpacing.y + mainTextContainer.height +
                                4 * hifi.dimensions.contentSpacing.y + form.height;

            parent.width = root.width = Math.max(d.minWidth, Math.min(d.maxWidth, targetWidth));
            parent.height = root.height = Math.max(d.minHeight, Math.min(d.maxHeight, targetHeight))
                + (keyboardEnabled && keyboardRaised ? (200 + 2 * hifi.dimensions.contentSpacing.y) : 0);
        }
    }

    function toggleLoading(isLoading) {
        linkAccountSpinner.visible = isLoading
        form.visible = !isLoading

        leftButton.visible = !isLoading
        buttons.visible = !isLoading
    }

    BusyIndicator {
        id: linkAccountSpinner

        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin: hifi.dimensions.contentSpacing.y
        }

        visible: false
        running: true

        width: 48
        height: 48
    }

    ShortcutText {
        id: mainTextContainer
        anchors {
            top: parent.top
            left: parent.left
            margins: 0
            topMargin: hifi.dimensions.contentSpacing.y
        }

        visible: false

        text: qsTr("There was an unknown error while creating your account.")
        wrapMode: Text.WordWrap
        color: hifi.colors.redAccent
        horizontalAlignment: Text.AlignLeft
    }

    Column {
        id: form
        width: parent.width
        onHeightChanged: d.resize(); onWidthChanged: d.resize();

        anchors {
            top: mainTextContainer.bottom
            topMargin: 2 * hifi.dimensions.contentSpacing.y
        }
        spacing: 2 * hifi.dimensions.contentSpacing.y

        TextField {
            id: emailField
            width: parent.width
            label: "Email"
            activeFocusOnPress: true
            onFocusChanged: {
                root.text = "";
            }
        }

        TextField {
            id: usernameField
            width: parent.width
            label: "Username"
            activeFocusOnPress: true

            ShortcutText {
                anchors {
                    verticalCenter: parent.textFieldLabel.verticalCenter
                    left: parent.textFieldLabel.right
                    leftMargin: 10
                }

                text: qsTr("No spaces / special chars.")

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter

                color: hifi.colors.blueAccent
                onFocusChanged: {
                    root.text = "";
                }
            }
        }

        TextField {
            id: passwordField
            width: parent.width
            label: "Password"
            echoMode: TextInput.Password
            activeFocusOnPress: true

            ShortcutText {
                anchors {
                    verticalCenter: parent.textFieldLabel.verticalCenter
                    left: parent.textFieldLabel.right
                    leftMargin: 10
                }

                text: qsTr("At least 6 characters")

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter

                color: hifi.colors.blueAccent
            }

            onFocusChanged: {
                root.text = "";
                root.isPassword = focus
            }
        }

        Row {
            id: leftButton
            anchors.horizontalCenter: parent.horizontalCenter

            spacing: hifi.dimensions.contentSpacing.x
            onHeightChanged: d.resize(); onWidthChanged: d.resize();

            Button {
                anchors.verticalCenter: parent.verticalCenter

                text: qsTr("Existing User")

                onClicked: {
                    bodyLoader.setSource("LinkAccountBody.qml")
                    if (!root.isTablet) {
                        bodyLoader.item.width = root.pane.width
                        bodyLoader.item.height = root.pane.height
                    }
                }
            }
        }

        Row {
            id: buttons
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: hifi.dimensions.contentSpacing.x
            onHeightChanged: d.resize(); onWidthChanged: d.resize();

            Button {
                id: linkAccountButton
                anchors.verticalCenter: parent.verticalCenter
                width: 200

                text: qsTr("Sign Up")
                color: hifi.buttons.blue

                onClicked: signupBody.signup()
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter

                text: qsTr("Cancel")

                onClicked: root.tryDestroy()
            }
        }
    }

    Component.onCompleted: {
        root.title = qsTr("Create an Account")
        root.iconText = "<"
        //dont rise local keyboard
        keyboardEnabled = !root.isTablet && HMD.active;
        //but rise Tablet's one instead for Tablet interface
        if (root.isTablet) {
            root.keyboardEnabled = HMD.active;
            root.keyboardRaised = Qt.binding( function() { return keyboardRaised; })
        }
        d.resize();

        emailField.forceActiveFocus();
    }

    Connections {
        target: loginDialog
        onHandleSignupCompleted: {
            console.log("Sign Up Succeeded");

            // now that we have an account, login with that username and password
            loginDialog.login(usernameField.text, passwordField.text)
        }
        onHandleSignupFailed: {
            console.log("Sign Up Failed")
            toggleLoading(false)

            mainTextContainer.text = errorString
            mainTextContainer.visible = true

            d.resize();
        }
        onHandleLoginCompleted: {
            bodyLoader.setSource("WelcomeBody.qml", { "welcomeBack": false })
            bodyLoader.item.width = root.pane.width
            bodyLoader.item.height = root.pane.height
        }
        onHandleLoginFailed: {
            // we failed to login, show the LoginDialog so the user will try again
            bodyLoader.setSource("LinkAccountBody.qml", { "failAfterSignUp": true })
            if (!root.isTablet) {
                bodyLoader.item.width = root.pane.width
                bodyLoader.item.height = root.pane.height
            }
        }
    }

    Keys.onPressed: {
        if (!visible) {
            return
        }

        switch (event.key) {
        case Qt.Key_Enter:
        case Qt.Key_Return:
            event.accepted = true
            signupBody.signup()
            break
        }
    }
}
