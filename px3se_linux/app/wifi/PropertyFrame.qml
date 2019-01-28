import QtQuick 2.3
import QtQuick.Controls 1.2


Rectangle {
    id: page

    property string ssid: ""
    property int signal: 0
    property string note: ""
    property string bssid: ""
    property string psk: ""    
    property bool is_connected: false
    property string status: ""
    signal changed(string value, string role)

    signal scan()

    signal connect()

    signal disconnect()

    Grid {
        anchors.fill: parent
        anchors.margins: 20
        columns: 2
        spacing: 5

        Text {id: lblNameStatus; text: "Status: "}
        TextField {
            text: status
            width: 200
            enabled: false
            onTextChanged: {
                if(status=="Completed"){
                    btnDisconnect.enabled=true
                    btnConnect.enabled=false
                    txt_psk.enabled=false;
                }else if(status=="Inactive"||status=="Scanning"||status=="Disconnected"){
                    btnDisconnect.enabled=false
                    btnConnect.enabled=true
                    txt_psk.enabled=true;
                }else{
                    btnDisconnect.enabled=false
                    btnConnect.enabled=false
                }

                if(ssid==""){
                    btnDisconnect.enabled=false
                    btnConnect.enabled=false
                }

                if(status=="Completed"){
                    btnDisconnect.enabled=true
                }

            }
        }

        Text {id: lblNameId; text: "SSID: "}
        TextField {
            text: ssid
            width: 200
            enabled: false
            onTextChanged: {
                if(ssid==""){
                    btnDisconnect.enabled=false
                    btnConnect.enabled=false
                }else{
                    btnDisconnect.enabled=false
                    btnConnect.enabled=true
                }

            }
        }
        Text {id: lblPhoneId; text: "PSK: "}
        TextField {
            id:txt_psk
            placeholderText: qsTr("Type the psk here")
            text: psk
            width: 200
            onTextChanged: {
                psk = text
                page.changed(text, "psk")
                if(psk==""||psk.length<8){
                    btnConnect.enabled=false
                }else{
                    btnConnect.enabled=true
                }
            }
        }

        Text {id: lblAddrId; text: "BSSID: "}
        TextField {            
            text: bssid
            width: 200
            enabled: false
            onTextChanged: {

            }
        }




        Button{
            text: qsTr("Scan")
            onClicked: {
                page.scan()
            }
        }

        Row{
            spacing: 5
            Button{
                id:btnConnect
                enabled: !is_connected
                text:qsTr("Connect")
                onClicked: {
                    page.connect()
                }
            }
            Button{
                id:btnDisconnect
                enabled: is_connected
                text:qsTr("DisConnect")
                onClicked: {
                    page.disconnect()
                }
            }
        }




    }

}
