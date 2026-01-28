#pragma once

#include "CoreMinimal.h"

#include <memory>
#include <vector>

#include "Kismet/KismetSystemLibrary.h"
#include "api/VehicleSimApiBase.hpp"
#include "AssetRegistry/AssetData.h"
#include "physics/PhysicsEngineBase.hpp"
#include "physics/World.hpp"
#include "physics/PhysicsWorld.hpp"
#include "common/StateReporterWrapper.hpp"
#include "api/ApiServerBase.hpp"
#include "SimModeBase.h"

#include "SimModeWorldBase.generated.h"

extern CORE_API uint32 GFrameNumber;

UCLASS()
class AIRSIM_API ASimModeWorldBase : public ASimModeBase
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    virtual void reset() override;
    virtual std::string getDebugReport() override;

    virtual bool isPaused() const override;
    virtual void pause(bool is_paused) override;
    virtual void continueForTime(double seconds) override;
    virtual void continueForFrames(uint32_t frames) override;

    virtual void setWind(const msr::airlib::Vector3r& wind) const override;
    virtual void setExtForce(const msr::airlib::Vector3r& ext_force) const override;

protected:
    void startAsyncUpdator();
    void stopAsyncUpdator();
    virtual void updateDebugReport(msr::airlib::StateReporterWrapper& debug_reporter) override;

    void initializeForPlay();
    virtual void registerPhysicsBody(msr::airlib::VehicleSimApiBase* physicsBody) override;

    long long getPhysicsLoopPeriod() const;
    void setPhysicsLoopPeriod(long long period);

private:
    typedef msr::airlib::UpdatableObject UpdatableObject;
    typedef msr::airlib::PhysicsEngineBase PhysicsEngineBase;
    typedef msr::airlib::ClockFactory ClockFactory;

    std::unique_ptr<PhysicsEngineBase> createPhysicsEngine();
    bool unreal_only_vehicles_reset_done_ = false;
    bool airlib_async_started_ = false;


private:
    std::unique_ptr<msr::airlib::PhysicsWorld> physics_world_;
    PhysicsEngineBase* physics_engine_ = nullptr;

    long long physics_loop_period_ = 3000000LL; //3ms
};
