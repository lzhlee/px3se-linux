import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Window 2.2
import QtQuick.VirtualKeyboard 2.1
import ApGui 1.0

ApplicationWindow {
    visible: true
    width:  Screen.width
    height:  Screen.height
    title: qsTr("Wifi")

    function listProperty(it)
    {
        for (var p in it) {
            console.log(p + ":" + it[p])
        }
    }



    Component {
        id: del
        Delegate {
            ssid: model.ssid
            signal: model.signal
            bssid: model.bssid
            onClicked: {                
                if(propFrame.status=="Inactive"||propFrame.status=="Scanning"){
                    propFrame.isUpdating = true
                    propFrame.ssid = model.ssid
                    propFrame.signal = model.signal
                    propFrame.note = model.note
                    propFrame.bssid = model.bssid
                    propFrame.psk = model.psk
                    propFrame.index = model.index
                    propFrame.isUpdating = false

                    modcpp.onNetWorkSelect(model.ssid,model.bssid,model.psk,model.flags);
                }
                //listProperty(modcpp)
            }
        }
    }

    ApGui{
        id:apGui
        onSig_ap_result: {
            switch(result){
                case 0:
                    console.log("xxxx");
                    break;
            }
        }
    }

    Button{
        anchors.top:parent.top
        anchors.right: parent.right
        text: "exit"
        onClicked: {
             Qt.quit();
        }
    }

    Column {
        id:clap
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 5
           spacing: 10
           Row{
               Label{
                   width: 150
                   text: qsTr("HostAp Name")
               }

               TextField {
                   id: text_ap_name
                   text:apGui.ap_name
                   width: 300
                   placeholderText: qsTr("Enter ap name")
                   onTextChanged: {
                        apGui.ap_name=text_ap_name.text
                   }
               }
           }
           Row{
               Label{
                   width: 150
                   text: qsTr("HostAp password")
               }

               TextField {
                   id: text_ap_password
                   anchors.leftMargin: 20
                   text:apGui.ap_password
                   width: 300
                   placeholderText: qsTr("Enter password")
                   onTextChanged: {
                        apGui.ap_password=text_ap_password.text
                   }
               }
           }
           Row{
               Label{
                   width: 150
                   text: qsTr("Start HostAp")
               }

               Switch {
                   anchors.leftMargin: 5
                   checked: apGui.ap_switch
                    onCheckedChanged: {
                        wifi.visible=!this.checked;
                        text_ap_name.enabled=!this.checked;
                        text_ap_password.enabled=!this.checked;
                        if(this.checked&&propFrame.is_connected){
                            propFrame.disconnect();
                        }

                        //apGui.ap_name=text_ap_name.text;
                        //apGui.ap_password=text_ap_password.text;
                        apGui.slot_ap_switch(this.checked);

                    }
               }
           }
       }

Rectangle {
    id:wifi
    width: parent.width
    height: parent.height- clap.height
    anchors.topMargin: 20
    anchors.left: parent.left
    anchors.top: clap.bottom


    ScrollView {
        id: lv

        width: 360
        anchors.top: parent.top
        height: parent.height

        ListView {
            model: modcpp
            delegate: del
        }
    }

    PropertyFrame {
        property int index: -1
        property bool isUpdating: false
        id: propFrame
        anchors.left: lv.right
        anchors.right: parent.right
        anchors.margins: 10
        height: lv.height
        y: 10
        onChanged: {
            if (!isUpdating) {
                modcpp.setItemData2(index, value, role);
            }            
        }
        onScan: {
            modcpp.scan();
        }

        onConnect: {
            propFrame.is_connected=true;
            modcpp.connectWifi();
        }

        onDisconnect: {
            propFrame.is_connected=false;
            modcpp.disconnectWifi();
        }
        Component.onCompleted: {
            modcpp.statusChange.connect(sendToPost)
        }
        function sendToPost(stat) {
            propFrame.status=stat;
            console.log("update status: " + stat)
        }

    }
}


    InputPanel {
           id: inputPanel
           y: Qt.inputMethod.visible ? parent.height - inputPanel.height : parent.height
           anchors.left: parent.left
           anchors.right: parent.right
       }

}
