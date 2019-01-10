/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                               *
 * Copyright (C) 2018 Simon Stürz <simon.stuerz@guh.io>                          *
 *                                                                               *
 * This file is part of nymea-networkmanager.                                    *
 *                                                                               *
 * nymea-networkmanager is free software: you can redistribute it and/or         *
 * modify it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation, either version 3 of the License,                *
 * or (at your option) any later version.                                        *
 *                                                                               *
 * nymea-networkmanager is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  *
 * GNU General Public License for more details.                                  *
 *                                                                               *
 * You should have received a copy of the GNU General Public License along       *
 * with nymea-networkmanager. If not, see <http://www.gnu.org/licenses/>.        *
 *                                                                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "wirelessservice.h"

#include "core.h"
#include "bluetoothuuids.h"
#include "loggingcategories.h"

#include <QJsonDocument>
#include <QNetworkInterface>
#include <QLowEnergyDescriptorData>
#include <QLowEnergyCharacteristicData>

WirelessService::WirelessService(QLowEnergyService *service, QObject *parent) :
    QObject(parent),
    m_service(service),
    m_readingInputData(false)
{    
    qCDebug(dcBluetoothServer()) << "Create WirelessService.";

    // Service
    connect(m_service, SIGNAL(characteristicChanged(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicChanged(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(characteristicRead(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicChanged(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(characteristicWritten(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicWritten(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(descriptorWritten(QLowEnergyDescriptor, QByteArray)), this, SLOT(descriptorWritten(QLowEnergyDescriptor, QByteArray)));
    connect(m_service, SIGNAL(error(QLowEnergyService::ServiceError)), this, SLOT(serviceError(QLowEnergyService::ServiceError)));
}

WirelessService::~WirelessService()
{
    qCDebug(dcBluetoothServer()) << "Delete wireless service";
}

QLowEnergyService *WirelessService::service()
{
    return m_service;
}

QLowEnergyServiceData WirelessService::serviceData()
{
    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(wirelessServiceUuid);

    QLowEnergyDescriptorData clientConfigDescriptorData(QBluetoothUuid::ClientCharacteristicConfiguration, QByteArray(2, 0));

    // Wifi commander characterisitc e081fec1-f757-4449-b9c9-bfa83133f7fc
    QLowEnergyCharacteristicData wirelessCommanderCharacteristicData;
    wirelessCommanderCharacteristicData.setUuid(wirelessCommanderCharacteristicUuid);
    wirelessCommanderCharacteristicData.setProperties(QLowEnergyCharacteristic::Write);
    wirelessCommanderCharacteristicData.setValueLength(0, 20);
    serviceData.addCharacteristic(wirelessCommanderCharacteristicData);

    // Response characterisitc e081fec2-f757-4449-b9c9-bfa83133f7fc
    QLowEnergyCharacteristicData wirelessResponseCharacteristicData;
    wirelessResponseCharacteristicData.setUuid(wirelessResponseCharacteristicUuid);
    wirelessResponseCharacteristicData.setProperties(QLowEnergyCharacteristic::Notify);
    wirelessResponseCharacteristicData.addDescriptor(clientConfigDescriptorData);
    wirelessResponseCharacteristicData.setValueLength(0, 20);
    serviceData.addCharacteristic(wirelessResponseCharacteristicData);

    // Wireless connection status characterisitc e081fec3-f757-4449-b9c9-bfa83133f7fc
    QLowEnergyCharacteristicData wirelessStatusCharacteristicData;
    wirelessStatusCharacteristicData.setUuid(wirelessStateCharacteristicUuid);
    wirelessStatusCharacteristicData.setProperties(QLowEnergyCharacteristic::Read | QLowEnergyCharacteristic::Notify);
    wirelessStatusCharacteristicData.addDescriptor(clientConfigDescriptorData);
    wirelessStatusCharacteristicData.setValueLength(1, 1);
    wirelessStatusCharacteristicData.setValue(WirelessService::getWirelessNetworkDeviceState(NetworkDevice::NetworkDeviceStateUnknown));
    serviceData.addCharacteristic(wirelessStatusCharacteristicData);

    // Wireless connection mode characterisitc e081fec4-f757-4449-b9c9-bfa83133f7fc
    QLowEnergyCharacteristicData wirelessModeCharacteristicData;
    wirelessModeCharacteristicData.setUuid(wirelessModeCharacteristicUuid);
    wirelessModeCharacteristicData.setProperties(QLowEnergyCharacteristic::Read | QLowEnergyCharacteristic::Notify);
    wirelessModeCharacteristicData.addDescriptor(clientConfigDescriptorData);
    wirelessModeCharacteristicData.setValueLength(1, 1);
    wirelessModeCharacteristicData.setValue(WirelessService::getWirelessMode(WirelessNetworkDevice::ModeUnknown));
    serviceData.addCharacteristic(wirelessModeCharacteristicData);

    return serviceData;
}


WirelessService::WirelessServiceResponse WirelessService::checkWirelessErrors()
{
    // Check possible errors
    if (!Core::instance()->networkManager()->available()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: The networkmanager is not available.";
        return WirelessServiceResponseNetworkManagerNotAvailable;
    }

    if (!Core::instance()->networkManager()->wirelessAvailable()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: There is no wireless device available.";
        return WirelessServiceResponseWirelessNotAvailable;
    }

    if (!Core::instance()->networkManager()->networkingEnabled()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Networking not enabled";
        return WirelessServiceResponseNetworkingNotEnabled;
    }

    if (!Core::instance()->networkManager()->wirelessEnabled()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Wireless not enabled";
        return WirelessServiceResponseWirelessNotEnabled;
    }

    return WirelessServiceResponseSuccess;
}

QByteArray WirelessService::getWirelessNetworkDeviceState(NetworkDevice::NetworkDeviceState state)
{
    switch (state) {
    case NetworkDevice::NetworkDeviceStateUnknown:
        return QByteArray::fromHex("00");
    case NetworkDevice::NetworkDeviceStateUnmanaged:
        return QByteArray::fromHex("01");
    case NetworkDevice::NetworkDeviceStateUnavailable:
        return QByteArray::fromHex("02");
    case NetworkDevice::NetworkDeviceStateDisconnected:
        return QByteArray::fromHex("03");
    case NetworkDevice::NetworkDeviceStatePrepare:
        return QByteArray::fromHex("04");
    case NetworkDevice::NetworkDeviceStateConfig:
        return QByteArray::fromHex("05");
    case NetworkDevice::NetworkDeviceStateNeedAuth:
        return QByteArray::fromHex("06");
    case NetworkDevice::NetworkDeviceStateIpConfig:
        return QByteArray::fromHex("07");
    case NetworkDevice::NetworkDeviceStateIpCheck:
        return QByteArray::fromHex("08");
    case NetworkDevice::NetworkDeviceStateSecondaries:
        return QByteArray::fromHex("09");
    case NetworkDevice::NetworkDeviceStateActivated:
        return QByteArray::fromHex("0a");
    case NetworkDevice::NetworkDeviceStateDeactivating:
        return QByteArray::fromHex("0b");
    case NetworkDevice::NetworkDeviceStateFailed:
        return QByteArray::fromHex("0c");
    }

    // Unknown
    return QByteArray::fromHex("00");
}

QByteArray WirelessService::getWirelessMode(WirelessNetworkDevice::Mode mode)
{
    switch (mode) {
    case WirelessNetworkDevice::ModeUnknown:
        return QByteArray::fromHex("00");
    case WirelessNetworkDevice::ModeAdhoc:
        return QByteArray::fromHex("01");
    case WirelessNetworkDevice::ModeInfrastructure:
        return QByteArray::fromHex("02");
    case WirelessNetworkDevice::ModeAccessPoint:
        return QByteArray::fromHex("03");
    }

    // Unknown
    return QByteArray::fromHex("00");
}


void WirelessService::streamData(const QVariantMap &responseMap)
{
    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    QByteArray data = QJsonDocument::fromVariant(responseMap).toJson(QJsonDocument::Compact) + '\n';
    qCDebug(dcBluetoothServer()) << "WirelessService: Start streaming response data:" << data.count() << "bytes";

    int sentDataLength = 0;
    QByteArray remainingData = data;
    while (!remainingData.isEmpty()) {
        QByteArray package = remainingData.left(20);
        sentDataLength += package.count();
        m_service->writeCharacteristic(characteristic, package);
        remainingData = remainingData.remove(0, package.count());
    }

    qCDebug(dcBluetoothServer()) << "WirelessService: Finished streaming response data";
}

QVariantMap WirelessService::createResponse(const WirelessService::WirelessServiceCommand &command, const WirelessService::WirelessServiceResponse &responseCode)
{
    QVariantMap response;
    response.insert("c", static_cast<int>(command));
    response.insert("r", static_cast<int>(responseCode));
    return response;
}

void WirelessService::commandGetNetworks(const QVariantMap &request)
{
    Q_UNUSED(request)

    if (!m_service) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Could not stream wireless network list. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    QVariantList accessPointVariantList;
    foreach (WirelessAccessPoint *accessPoint, Core::instance()->networkManager()->wirelessNetworkDevices().first()->accessPoints()) {
        QVariantMap accessPointVariantMap;
        accessPointVariantMap.insert("e", accessPoint->ssid());
        accessPointVariantMap.insert("m", accessPoint->macAddress());
        accessPointVariantMap.insert("s", accessPoint->signalStrength());
        accessPointVariantMap.insert("p", static_cast<int>(accessPoint->isProtected()));
        accessPointVariantList.append(accessPointVariantMap);
    }

    QVariantMap response = createResponse(WirelessServiceCommandGetNetworks);
    response.insert("p", accessPointVariantList);

    streamData(response);
}

void WirelessService::commandConnect(const QVariantMap &request)
{
    if (!m_service) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Could not stream wireless network list. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    if (!request.contains("p")) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Connect command: Missing parameters.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidParameters));
        return;
    }

    QVariantMap parameters = request.value("p").toMap();
    if (!parameters.contains("e") || !parameters.contains("p")) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Connect command: Invalid parameters.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidParameters));
        return;
    }

    Core::instance()->networkManager()->connectWifi(Core::instance()->networkManager()->wirelessNetworkDevices().first()->interface(), parameters.value("e").toString(), parameters.value("p").toString());
    streamData(createResponse(WirelessServiceCommandConnect));
}

void WirelessService::commandConnectHidden(const QVariantMap &request)
{
    Q_UNUSED(request)
    // TODO:
    qCWarning(dcBluetoothServer()) << "Connect to hidden network is not implemented yet.";
}

void WirelessService::commandDisconnect(const QVariantMap &request)
{
    Q_UNUSED(request)

    if (!m_service) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    Core::instance()->networkManager()->wirelessNetworkDevices().first()->disconnectDevice();
    streamData(createResponse(WirelessServiceCommandDisconnect));
}

void WirelessService::commandScan(const QVariantMap &request)
{
    Q_UNUSED(request)

    qCDebug(dcBluetoothServer()) << "WirelessService: Execute command scan.";

    if (!m_service) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Could scan wireless networks. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    Core::instance()->networkManager()->wirelessNetworkDevices().first()->scanWirelessNetworks();
    streamData(createResponse(WirelessServiceCommandScan));
}

void WirelessService::commandGetCurrentConnection(const QVariantMap &request)
{
    Q_UNUSED(request)
    qCDebug(dcBluetoothServer()) << "WirelessService: Execute get current connection.";

    if (!m_service) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    WirelessNetworkDevice *device = Core::instance()->networkManager()->wirelessNetworkDevices().first();

    QVariantMap connectionDataMap;
    QNetworkInterface wifiInterface = QNetworkInterface::interfaceFromName(device->interface());
    if (!device->activeAccessPoint() || !wifiInterface.isValid() || wifiInterface.addressEntries().isEmpty()) {
        qCDebug(dcBluetoothServer()) << "WirelessService: There is currently no access active accesspoint";
        connectionDataMap.insert("e", "");
        connectionDataMap.insert("m", "");
        connectionDataMap.insert("s", 0);
        connectionDataMap.insert("p", 0);
        connectionDataMap.insert("i", "");
    } else {
        QHostAddress address = wifiInterface.addressEntries().first().ip();
        qCDebug(dcBluetoothServer()) << "WirelessService: Current connection:" << device->activeAccessPoint() << address.toString();
        connectionDataMap.insert("e", device->activeAccessPoint()->ssid());
        connectionDataMap.insert("m", device->activeAccessPoint()->macAddress());
        connectionDataMap.insert("s", device->activeAccessPoint()->signalStrength());
        connectionDataMap.insert("p", static_cast<int>(device->activeAccessPoint()->isProtected()));
        connectionDataMap.insert("i", address.toString());
    }

    QVariantMap response = createResponse(WirelessServiceCommandGetCurrentConnection);
    response.insert("p", connectionDataMap);
    streamData(response);
}

void WirelessService::commandStartAccessPoint(const QVariantMap &request)
{
    if (!m_service) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Could not stream wireless network list. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    if (!request.contains("p")) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Connect command: Missing parameters.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidParameters));
        return;
    }

    QVariantMap parameters = request.value("p").toMap();
    if (!parameters.contains("e") || !parameters.contains("p")) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Connect command: Invalid parameters.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidParameters));
        return;
    }

    Core::instance()->networkManager()->startAccessPoint(Core::instance()->networkManager()->wirelessNetworkDevices().first()->interface(), parameters.value("e").toString(), parameters.value("p").toString());
    streamData(createResponse(WirelessServiceCommandStartAccessPoint));
}

void WirelessService::characteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    // Command
    if (characteristic.uuid() == wirelessCommanderCharacteristicUuid) {
        // Check if currently reading
        if (m_readingInputData) {
            m_inputDataStream.append(value);
        } else {
            m_inputDataStream.clear();
            m_readingInputData = true;
            m_inputDataStream.append(value);
        }

        // If command finished
        if (value.endsWith('\n')) {
            QJsonParseError error;
            QJsonDocument jsonDocument = QJsonDocument::fromJson(m_inputDataStream, &error);
            if (error.error != QJsonParseError::NoError) {
                qCWarning(dcBluetoothServer()) << "Got invalid json object" << m_inputDataStream;
                m_inputDataStream.clear();
                m_readingInputData = false;
                return;
            }

            qCDebug(dcBluetoothServer()) << "Got command stream" << jsonDocument.toJson();

            processCommand(jsonDocument.toVariant().toMap());

            m_inputDataStream.clear();
            m_readingInputData = false;
        }

        // Limit possible data stream to prevent overflow
        if (m_inputDataStream.length() >= 20 * 1024) {
            m_inputDataStream.clear();
            m_readingInputData = false;
            return;
        }
    }
}

void WirelessService::characteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qCDebug(dcBluetoothServer()) << "WirelessService: Characteristic read" << characteristic.uuid().toString() << value;
}

void WirelessService::characteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qCDebug(dcBluetoothServer()) << "WirelessService: Characteristic written" << characteristic.uuid().toString() << value;
}

void WirelessService::descriptorRead(const QLowEnergyDescriptor &descriptor, const QByteArray &value)
{
    qCDebug(dcBluetoothServer()) << "WirelessService: Descriptor read" << descriptor.uuid().toString() << value;
}

void WirelessService::descriptorWritten(const QLowEnergyDescriptor &descriptor, const QByteArray &value)
{
    qCDebug(dcBluetoothServer()) << "WirelessService: Descriptor written" << descriptor.uuid().toString() << value;
}

void WirelessService::serviceError(const QLowEnergyService::ServiceError &error)
{
    QString errorString;
    switch (error) {
    case QLowEnergyService::NoError:
        errorString = "No error";
        break;
    case QLowEnergyService::OperationError:
        errorString = "Operation error";
        break;
    case QLowEnergyService::CharacteristicReadError:
        errorString = "Characteristic read error";
        break;
    case QLowEnergyService::CharacteristicWriteError:
        errorString = "Characteristic write error";
        break;
    case QLowEnergyService::DescriptorReadError:
        errorString = "Descriptor read error";
        break;
    case QLowEnergyService::DescriptorWriteError:
        errorString = "Descriptor write error";
        break;
    case QLowEnergyService::UnknownError:
        errorString = "Unknown error";
        break;
    }

    qCWarning(dcBluetoothServer()) << "WirelessService: Error:" << errorString;
}

void WirelessService::processCommand(const QVariantMap &request)
{
    if (!request.contains("c")) {
        qCWarning(dcBluetoothServer()) << "Invalid request. Command value missing.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidCommand));
        return;
    }

    bool commandIntOk;
    int command = request.value("c").toInt(&commandIntOk);
    if (!commandIntOk) {
        qCWarning(dcBluetoothServer()) << "Invalid request. Could not convert method to interger.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidCommand));
        return;
    }

    // Check wireless errors
    WirelessServiceResponse responseCode = checkWirelessErrors();
    if (responseCode != WirelessServiceResponseSuccess) {
        streamData(createResponse(static_cast<WirelessServiceCommand>(command), responseCode));
        return;
    }

    // Process method
    switch (command) {
    case WirelessServiceCommandGetNetworks:
        commandGetNetworks(request);
        break;
    case WirelessServiceCommandConnect:
        commandConnect(request);
        break;
    case WirelessServiceCommandConnectHidden:
        commandConnectHidden(request);
        break;
    case WirelessServiceCommandDisconnect:
        commandDisconnect(request);
        break;
    case WirelessServiceCommandScan:
        commandScan(request);
        break;
    case WirelessServiceCommandGetCurrentConnection:
        commandGetCurrentConnection(request);
        break;
    case WirelessServiceCommandStartAccessPoint:
        commandStartAccessPoint(request);
        break;
    default:
        qCWarning(dcBluetoothServer()) << "Invalid request. Unknown command" << command;
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidCommand));
        break;
    }
}

void WirelessService::onWirelessDeviceBitRateChanged(int bitRate)
{
    Q_UNUSED(bitRate)
}

void WirelessService::onWirelessModeChanged(WirelessNetworkDevice::Mode mode)
{
    if (!m_service) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Could not update wireless device mode. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessModeCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Could not update wireless device mode. Characteristic not valid";
        return;
    }

    qCDebug(dcBluetoothServer()) << "WirelessService: Notify wireless mode changed" << WirelessService::getWirelessMode(mode);
    m_service->writeCharacteristic(characteristic, WirelessService::getWirelessMode(mode));
}

void WirelessService::onWirelessDeviceStateChanged(NetworkDevice::NetworkDeviceState state)
{
    if (!m_service) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Could not update wireless network device state. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessStateCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcBluetoothServer()) << "WirelessService: Could not update wireless network device state. Characteristic not valid";
        return;
    }

    qCDebug(dcBluetoothServer()) << "WirelessService: Notify wireless state changed" << WirelessService::getWirelessNetworkDeviceState(state);
    m_service->writeCharacteristic(characteristic, WirelessService::getWirelessNetworkDeviceState(state));
}
