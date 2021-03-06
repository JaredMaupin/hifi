//
//  AvatarHashMap.cpp
//  libraries/avatars/src
//
//  Created by Andrew Meadows on 1/28/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QtCore/QDataStream>

#include <NodeList.h>
#include <udt/PacketHeaders.h>
#include <PerfStat.h>
#include <SharedUtil.h>

#include "AvatarLogging.h"
#include "AvatarHashMap.h"

AvatarHashMap::AvatarHashMap() {
    auto nodeList = DependencyManager::get<NodeList>();

    connect(nodeList.data(), &NodeList::uuidChanged, this, &AvatarHashMap::sessionUUIDChanged);
}

QVector<QUuid> AvatarHashMap::getAvatarIdentifiers() {
    QReadLocker locker(&_hashLock);
    return _avatarHash.keys().toVector();
}

bool AvatarHashMap::isAvatarInRange(const glm::vec3& position, const float range) {
    auto hashCopy = getHashCopy();
    foreach(const AvatarSharedPointer& sharedAvatar, hashCopy) {
        glm::vec3 avatarPosition = sharedAvatar->getWorldPosition();
        float distance = glm::distance(avatarPosition, position);
        if (distance < range) {
            return true;
        }
    }
    return false;
}

int AvatarHashMap::numberOfAvatarsInRange(const glm::vec3& position, float rangeMeters) {
    auto hashCopy = getHashCopy();
    auto rangeMeters2 = rangeMeters * rangeMeters;
    int count = 0;
    for (const AvatarSharedPointer& sharedAvatar : hashCopy) {
        glm::vec3 avatarPosition = sharedAvatar->getWorldPosition();
        auto distance2 = glm::distance2(avatarPosition, position);
        if (distance2 < rangeMeters2) {
            ++count;
        }
    }
    return count;
}

AvatarSharedPointer AvatarHashMap::newSharedAvatar() {
    return std::make_shared<AvatarData>();
}

AvatarSharedPointer AvatarHashMap::addAvatar(const QUuid& sessionUUID, const QWeakPointer<Node>& mixerWeakPointer) {
    qCDebug(avatars) << "Adding avatar with sessionUUID " << sessionUUID << "to AvatarHashMap.";

    auto avatar = newSharedAvatar();
    avatar->setSessionUUID(sessionUUID);
    avatar->setOwningAvatarMixer(mixerWeakPointer);

    _avatarHash.insert(sessionUUID, avatar);
    emit avatarAddedEvent(sessionUUID);

    return avatar;
}

AvatarSharedPointer AvatarHashMap::newOrExistingAvatar(const QUuid& sessionUUID, const QWeakPointer<Node>& mixerWeakPointer) {
    QWriteLocker locker(&_hashLock);
    auto avatar = _avatarHash.value(sessionUUID);
    if (!avatar) {
        avatar = addAvatar(sessionUUID, mixerWeakPointer);
    }
    return avatar;
}

AvatarSharedPointer AvatarHashMap::findAvatar(const QUuid& sessionUUID) const {
    QReadLocker locker(&_hashLock);
    if (_avatarHash.contains(sessionUUID)) {
        return _avatarHash.value(sessionUUID);
    }
    return nullptr;
}

void AvatarHashMap::processAvatarDataPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {
    PerformanceTimer perfTimer("receiveAvatar");
    // enumerate over all of the avatars in this packet
    // only add them if mixerWeakPointer points to something (meaning that mixer is still around)
    while (message->getBytesLeftToRead()) {
        parseAvatarData(message, sendingNode);
    }
}

AvatarSharedPointer AvatarHashMap::parseAvatarData(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {
    QUuid sessionUUID = QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID));

    int positionBeforeRead = message->getPosition();

    QByteArray byteArray = message->readWithoutCopy(message->getBytesLeftToRead());

    // make sure this isn't our own avatar data or for a previously ignored node
    auto nodeList = DependencyManager::get<NodeList>();

    if (sessionUUID != _lastOwnerSessionUUID && (!nodeList->isIgnoringNode(sessionUUID) || nodeList->getRequestsDomainListData())) {
        auto avatar = newOrExistingAvatar(sessionUUID, sendingNode);

        // have the matching (or new) avatar parse the data from the packet
        int bytesRead = avatar->parseDataFromBuffer(byteArray);
        message->seek(positionBeforeRead + bytesRead);
        return avatar;
    } else {
        // create a dummy AvatarData class to throw this data on the ground
        AvatarData dummyData;
        int bytesRead = dummyData.parseDataFromBuffer(byteArray);
        message->seek(positionBeforeRead + bytesRead);
        return std::make_shared<AvatarData>();
    }
}

void AvatarHashMap::processAvatarIdentityPacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {

    // peek the avatar UUID from the incoming packet
    QUuid identityUUID = QUuid::fromRfc4122(message->peek(NUM_BYTES_RFC4122_UUID));

    if (identityUUID.isNull()) {
        qCDebug(avatars) << "Refusing to process identity packet for null avatar ID";
        return;
    }

    // make sure this isn't for an ignored avatar
    auto nodeList = DependencyManager::get<NodeList>();
    static auto EMPTY = QUuid();

    {
        QReadLocker locker(&_hashLock);
        auto me = _avatarHash.find(EMPTY);
        if ((me != _avatarHash.end()) && (identityUUID == me.value()->getSessionUUID())) {
            // We add MyAvatar to _avatarHash with an empty UUID. Code relies on this. In order to correctly handle an
            // identity packet for ourself (such as when we are assigned a sessionDisplayName by the mixer upon joining),
            // we make things match here.
            identityUUID = EMPTY;
        }
    }
    
    if (!nodeList->isIgnoringNode(identityUUID) || nodeList->getRequestsDomainListData()) {
        // mesh URL for a UUID, find avatar in our list
        auto avatar = newOrExistingAvatar(identityUUID, sendingNode);
        bool identityChanged = false;
        bool displayNameChanged = false;
        bool skeletonModelUrlChanged = false;
        // In this case, the "sendingNode" is the Avatar Mixer.
        avatar->processAvatarIdentity(message->getMessage(), identityChanged, displayNameChanged, skeletonModelUrlChanged);
    }
}

void AvatarHashMap::processKillAvatar(QSharedPointer<ReceivedMessage> message, SharedNodePointer sendingNode) {
    // read the node id
    QUuid sessionUUID = QUuid::fromRfc4122(message->readWithoutCopy(NUM_BYTES_RFC4122_UUID));

    KillAvatarReason reason;
    message->readPrimitive(&reason);
    removeAvatar(sessionUUID, reason);
}

void AvatarHashMap::removeAvatar(const QUuid& sessionUUID, KillAvatarReason removalReason) {
    QWriteLocker locker(&_hashLock);

    auto removedAvatar = _avatarHash.take(sessionUUID);

    if (removedAvatar) {
        handleRemovedAvatar(removedAvatar, removalReason);
    }
}

void AvatarHashMap::handleRemovedAvatar(const AvatarSharedPointer& removedAvatar, KillAvatarReason removalReason) {
    qCDebug(avatars) << "Removed avatar with UUID" << uuidStringWithoutCurlyBraces(removedAvatar->getSessionUUID())
        << "from AvatarHashMap" << removalReason;
    emit avatarRemovedEvent(removedAvatar->getSessionUUID());
}

void AvatarHashMap::sessionUUIDChanged(const QUuid& sessionUUID, const QUuid& oldUUID) {
    _lastOwnerSessionUUID = oldUUID;
    emit avatarSessionChangedEvent(sessionUUID, oldUUID);
}

