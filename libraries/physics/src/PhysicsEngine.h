//
//  PhysicsEngine.h
//  libraries/physics/src
//
//  Created by Andrew Meadows 2014.10.29
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PhysicsEngine_h
#define hifi_PhysicsEngine_h

#include <stdint.h>
#include <set>
#include <vector>

#include <QUuid>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include "BulletUtil.h"
#include "ContactInfo.h"
#include "ObjectMotionState.h"
#include "ThreadSafeDynamicsWorld.h"
#include "ObjectAction.h"
#include "ObjectConstraint.h"

const float HALF_SIMULATION_EXTENT = 512.0f; // meters

class CharacterController;

// simple class for keeping track of contacts
class ContactKey {
public:
    ContactKey() = delete;
    ContactKey(void* a, void* b) : _a(a), _b(b) {}
    bool operator<(const ContactKey& other) const { return _a < other._a || (_a == other._a && _b < other._b); }
    bool operator==(const ContactKey& other) const { return _a == other._a && _b == other._b; }
    void* _a; // ObjectMotionState pointer
    void* _b; // ObjectMotionState pointer
};

using ContactMap = std::map<ContactKey, ContactInfo>;
using CollisionEvents = std::vector<Collision>;

class PhysicsEngine {
public:
    PhysicsEngine(const glm::vec3& offset);
    ~PhysicsEngine();
    void init();

    uint32_t getNumSubsteps();

    void removeObjects(const VectorOfMotionStates& objects);
    void removeSetOfObjects(const SetOfMotionStates& objects); // only called during teardown

    void addObjects(const VectorOfMotionStates& objects);
    VectorOfMotionStates changeObjects(const VectorOfMotionStates& objects);
    void reinsertObject(ObjectMotionState* object);

    void stepSimulation();
    void harvestPerformanceStats();
    void printPerformanceStatsToFile(const QString& filename);
    void updateContactMap();
    void doOwnershipInfectionForConstraints();

    bool hasOutgoingChanges() const { return _hasOutgoingChanges; }

    /// \return reference to list of changed MotionStates.  The list is only valid until beginning of next simulation loop.
    const VectorOfMotionStates& getChangedMotionStates();
    const VectorOfMotionStates& getDeactivatedMotionStates() const { return _dynamicsWorld->getDeactivatedMotionStates(); }

    /// \return reference to list of Collision events.  The list is only valid until beginning of next simulation loop.
    const CollisionEvents& getCollisionEvents();

    /// \brief prints timings for last frame if stats have been requested.
    void dumpStatsIfNecessary();

    /// \brief saves timings for last frame in filename
    void saveNextPhysicsStats(QString filename);

    /// \param offset position of simulation origin in domain-frame
    void setOriginOffset(const glm::vec3& offset) { _originOffset = offset; }

    /// \return position of simulation origin in domain-frame
    const glm::vec3& getOriginOffset() const { return _originOffset; }

    void setCharacterController(CharacterController* character);

    void dumpNextStats() { _dumpNextStats = true; }

    EntityDynamicPointer getDynamicByID(const QUuid& dynamicID) const;
    bool addDynamic(EntityDynamicPointer dynamic);
    void removeDynamic(const QUuid dynamicID);
    void forEachDynamic(std::function<void(EntityDynamicPointer)> actor);

private:
    QList<EntityDynamicPointer> removeDynamicsForBody(btRigidBody* body);
    void addObjectToDynamicsWorld(ObjectMotionState* motionState);

    /// \brief bump any objects that touch this one, then remove contact info
    void bumpAndPruneContacts(ObjectMotionState* motionState);

    void removeContacts(ObjectMotionState* motionState);

    void doOwnershipInfection(const btCollisionObject* objectA, const btCollisionObject* objectB);

    btClock _clock;
    btDefaultCollisionConfiguration* _collisionConfig = NULL;
    btCollisionDispatcher* _collisionDispatcher = NULL;
    btBroadphaseInterface* _broadphaseFilter = NULL;
    btSequentialImpulseConstraintSolver* _constraintSolver = NULL;
    ThreadSafeDynamicsWorld* _dynamicsWorld = NULL;
    btGhostPairCallback* _ghostPairCallback = NULL;

    ContactMap _contactMap;
    CollisionEvents _collisionEvents;
    QHash<QUuid, EntityDynamicPointer> _objectDynamics;
    QHash<btRigidBody*, QSet<QUuid>> _objectDynamicsByBody;
    std::set<btRigidBody*> _activeStaticBodies;
    QString _statsFilename;

    glm::vec3 _originOffset;

    CharacterController* _myAvatarController;

    uint32_t _numContactFrames = 0;
    uint32_t _numSubsteps;

    bool _dumpNextStats { false };
    bool _saveNextStats { false };
    bool _hasOutgoingChanges { false };

};

typedef std::shared_ptr<PhysicsEngine> PhysicsEnginePointer;

#endif // hifi_PhysicsEngine_h
