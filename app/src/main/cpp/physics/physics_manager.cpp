#include "physics_manager.h"
#include <android/log.h>
#include <cmath>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PhysicsManager", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PhysicsManager", __VA_ARGS__)

namespace oblivion {

bool PhysicsManager::init() {
    if (physicsSystem) {
        LOGI("Jolt Physics already initialized, skipping");
        return true;
    }

    LOGI("Initializing Jolt Physics...");

    // RegisterDefaultAllocator and RegisterTypes are global one-time operations.
    // Only call them if Factory::sInstance is not yet set up.
    // NOTE: JPH::RegisterTypes() can SIGTRAP on certain Android emulators.
    // Skip full initialization; physicsSystem remains null and update() is a no-op.
    if (JPH::Factory::sInstance == nullptr) {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        // Skip JPH::RegisterTypes() to avoid SIGTRAP on emulators
    }

    LOGI("Jolt Physics skipped (emulator workaround)");
    return true;
}

void PhysicsManager::update(float deltaTime) {
    if (!physicsSystem) return;

    // Stable simulation with fixed timestep
    accumulator += deltaTime;
    while (accumulator >= FIXED_TIMESTEP) {
        physicsSystem->Update(FIXED_TIMESTEP, 1, tempAllocator, jobSystem);
        accumulator -= FIXED_TIMESTEP;
    }
}

void PhysicsManager::shutdown() {
    LOGI("Shutting down Jolt Physics...");
    if (physicsSystem) {
        delete physicsSystem;
        physicsSystem = nullptr;
    }
    delete jobSystem;
    jobSystem = nullptr;
    delete tempAllocator;
    tempAllocator = nullptr;
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    JPH::UnregisterTypes();
    LOGI("Jolt Physics shutdown complete");
}

void PhysicsManager::createTerrainFromLand(const float* heightData, int size, float cellX, float cellY, float cellWorldSize) {
    if (!heightData || size <= 0) {
        LOGE("Invalid terrain data");
        return;
    }

    // Convert height data to float array
    uint32_t sampleCount = static_cast<uint32_t>(size);
    std::vector<float> heights(sampleCount * sampleCount);
    for (uint32_t i = 0; i < sampleCount * sampleCount; ++i) {
        heights[i] = heightData[i];
    }

    const float sampleSpacing = cellWorldSize / (size - 1);

    JPH::HeightFieldShapeSettings settings(
        heights.data(),
        JPH::Vec3(cellX * cellWorldSize, 0.0f, cellY * cellWorldSize),
        JPH::Vec3(sampleSpacing, 1.0f, sampleSpacing),
        sampleCount
    );
    settings.mBitsPerSample = 16;

    JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (!result.IsValid()) {
        LOGE("Failed to create HeightFieldShape");
        return;
    }

    JPH::BodyCreationSettings bodySettings(
        result.Get(),
        JPH::Vec3::sZero(),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        static_cast<JPH::ObjectLayer>(PhysicsLayer::NON_MOVING)
    );

    physicsSystem->GetBodyInterface().CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);
    LOGI("Terrain created for cell (%.0f, %.0f)", cellX, cellY);
}

JPH::CharacterVirtual* PhysicsManager::createCharacter(const glm::vec3& position, float height, float radius) {
    if (!physicsSystem) return nullptr;

    // Capsule shape (CharacterVirtual recommended)
    JPH::RefConst<JPH::Shape> shape = new JPH::CapsuleShape(height * 0.5f, radius);

    JPH::CharacterVirtualSettings settings;
    settings.mMass = 80.0f;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
    settings.mMaxStrength = 100.0f;
    settings.mShape = shape;
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius); // Ground detection from feet

    auto* character = new JPH::CharacterVirtual(
        &settings,
        JPH::Vec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(),
        physicsSystem
    );

    LOGI("CharacterVirtual created at (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
    return character;
}

void PhysicsManager::updateCharacter(JPH::CharacterVirtual* character, float deltaTime, const glm::vec3& input) {
    if (!character || !physicsSystem) return;

    // Convert input to movement velocity (close to Oblivion walk speed)
    const float moveSpeed = 4.0f;
    JPH::Vec3 currentVel = character->GetLinearVelocity();
    currentVel.SetX(input.x * moveSpeed);
    currentVel.SetZ(input.z * moveSpeed);

    // Gravity is processed inside CharacterVirtual, but Y velocity is maintained
    character->SetLinearVelocity(currentVel);

    // Extended update (ground detection, slope, stairs, wall penetration prevention)
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    character->ExtendedUpdate(
        deltaTime,
        physicsSystem->GetGravity(),
        updateSettings,
        physicsSystem->GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsLayer::MOVING)),
        physicsSystem->GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsLayer::MOVING)),
        {}, {},
        *tempAllocator
    );
}

glm::vec3 PhysicsManager::getCharacterPosition(JPH::CharacterVirtual* character) const {
    if (!character) return glm::vec3(0.0f, 0.0f, 0.0f);
    JPH::Vec3 pos = character->GetPosition();
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

bool PhysicsManager::isCharacterGrounded(JPH::CharacterVirtual* character) const {
    if (!character) return false;
    return character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
}

void PhysicsManager::destroyCharacter(JPH::CharacterVirtual* character) {
    delete character;
}

JPH::BodyID PhysicsManager::createBox(const glm::vec3& pos, const glm::vec3& halfExtents, float mass) {
    if (!physicsSystem) return JPH::BodyID();

    JPH::BodyCreationSettings settings(
        new JPH::BoxShape(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z)),
        JPH::Vec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        mass > 0.0f ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
        mass > 0.0f ? static_cast<JPH::ObjectLayer>(PhysicsLayer::MOVING)
                    : static_cast<JPH::ObjectLayer>(PhysicsLayer::NON_MOVING)
    );

    if (mass > 0.0f) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }

    return physicsSystem->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

JPH::BodyID PhysicsManager::createSphere(const glm::vec3& pos, float radius, float mass) {
    if (!physicsSystem) return JPH::BodyID();

    JPH::BodyCreationSettings settings(
        new JPH::SphereShape(radius),
        JPH::Vec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        mass > 0.0f ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
        mass > 0.0f ? static_cast<JPH::ObjectLayer>(PhysicsLayer::MOVING)
                    : static_cast<JPH::ObjectLayer>(PhysicsLayer::NON_MOVING)
    );

    if (mass > 0.0f) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }

    return physicsSystem->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

void PhysicsManager::setBodyPosition(JPH::BodyID bodyId, const glm::vec3& pos) {
    if (!physicsSystem || bodyId.IsInvalid()) return;
    physicsSystem->GetBodyInterface().SetPosition(bodyId, JPH::Vec3(pos.x, pos.y, pos.z), JPH::EActivation::Activate);
}

glm::vec3 PhysicsManager::getBodyPosition(JPH::BodyID bodyId) const {
    if (!physicsSystem || bodyId.IsInvalid()) return glm::vec3(0.0f, 0.0f, 0.0f);
    JPH::Vec3 pos = physicsSystem->GetBodyInterface().GetPosition(bodyId);
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

void PhysicsManager::removeBody(JPH::BodyID bodyId) {
    if (!physicsSystem || bodyId.IsInvalid()) return;
    physicsSystem->GetBodyInterface().RemoveBody(bodyId);
    physicsSystem->GetBodyInterface().DestroyBody(bodyId);
}

bool PhysicsManager::raycast(const Ray& ray, RaycastHit& hit) {
    if (!physicsSystem) return false;

    JPH::RRayCast rayCast(
        JPH::Vec3(ray.origin.x, ray.origin.y, ray.origin.z),
        JPH::Vec3(ray.direction.x, ray.direction.y, ray.direction.z) * ray.maxDistance
    );

    JPH::RayCastResult result;
    if (physicsSystem->GetNarrowPhaseQuery().CastRay(rayCast, result)) {
        hit.hit = true;
        hit.distance = result.mFraction * ray.maxDistance;
        JPH::Vec3 hitPoint = rayCast.GetPointOnRay(result.mFraction);
        hit.point = glm::vec3(hitPoint.GetX(), hitPoint.GetY(), hitPoint.GetZ());

        // Get normal (safely with BodyLock)
        JPH::BodyLockRead lock(physicsSystem->GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, hitPoint);
            hit.normal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }
        return true;
    }

    hit.hit = false;
    return false;
}

} // namespace oblivion
