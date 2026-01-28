#include "SimModeWorldBase.h"

#include "physics/FastPhysicsEngine.hpp"
#include "physics/ExternalPhysicsEngine.hpp"
#include "AirBlueprintLib.h"
#include "common/Settings.hpp"

#include <exception>
#include <algorithm>
#include <cctype>
#include <string>


static bool isAirLibManagedVehicle(const msr::airlib::VehicleSimApiBase* api)
{
    if (api == nullptr)
        return true; 

    const std::string vehicle_name = api->getVehicleName();

    msr::airlib::Settings vehicles_settings;
    if (!msr::airlib::Settings::singleton().getChild("Vehicles", vehicles_settings))
        return true;

    msr::airlib::Settings vehicle_settings;
    if (!vehicles_settings.getChild(vehicle_name, vehicle_settings))
        return true;

    std::string vehicle_type = vehicle_settings.getString("VehicleType", "");

    std::string t = vehicle_type;
    std::transform(t.begin(), t.end(), t.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (t.find("husky") != std::string::npos ||
        t.find("car") != std::string::npos ||
        t.find("rover") != std::string::npos ||
        t.find("skid") != std::string::npos ||
        t.find("physx") != std::string::npos)
    {
        return false;
    }

    if (t.find("simpleflight") != std::string::npos ||
        t.find("px4") != std::string::npos ||
        t.find("ardu") != std::string::npos ||
        t.find("multirotor") != std::string::npos)
    {
        return true;
    }

    return true;
}

void ASimModeWorldBase::BeginPlay()
{
    Super::BeginPlay();

    if (!physics_world_) {
        initializeForPlay();
        UAirBlueprintLib::LogMessageString("WorldBase: initializeForPlay() called once", "", LogDebugLevel::Informational);
    }

    if (physics_world_ && !airlib_async_started_) {
        startAsyncUpdator();
        airlib_async_started_ = true;
    }
}



void ASimModeWorldBase::initializeForPlay()
{
    std::vector<msr::airlib::UpdatableObject*> vehicles;

    for (auto& api : getApiProvider()->getVehicleSimApis())
    {
        if (isAirLibManagedVehicle(api)) {
            vehicles.push_back(api);
        }
        else {
            UAirBlueprintLib::LogMessageString(
                "BothMode: letting Unreal handle vehicle (skip AirLib PhysicsWorld): ",
                api->getVehicleName(),
                LogDebugLevel::Informational);

            if (!unreal_only_vehicles_reset_done_) {
                api->reset(); 
            }
        }
    }

    unreal_only_vehicles_reset_done_ = true;

    std::unique_ptr<PhysicsEngineBase> physics_engine = createPhysicsEngine();
    physics_engine_ = physics_engine.get();

    physics_world_.reset(new msr::airlib::PhysicsWorld(
        std::move(physics_engine),
        vehicles,
        getPhysicsLoopPeriod()));
}


void ASimModeWorldBase::registerPhysicsBody(msr::airlib::VehicleSimApiBase* physicsBody)
{

    if (!isAirLibManagedVehicle(physicsBody)) {
        UAirBlueprintLib::LogMessageString(
            "registerPhysicsBody: skip (Unreal handles this vehicle): ",
            physicsBody ? physicsBody->getVehicleName() : "<null>",
            LogDebugLevel::Informational);
        return;
    }

    physics_world_.get()->addBody(physicsBody);
}

void ASimModeWorldBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    unreal_only_vehicles_reset_done_ = false;

    if (physics_world_ && airlib_async_started_) {
        stopAsyncUpdator();
        airlib_async_started_ = false;
    }

    physics_world_.reset();
    Super::EndPlay(EndPlayReason);
}


void ASimModeWorldBase::startAsyncUpdator()
{
    physics_world_->startAsyncUpdator();
}

void ASimModeWorldBase::stopAsyncUpdator()
{
    physics_world_->stopAsyncUpdator();
}

long long ASimModeWorldBase::getPhysicsLoopPeriod() const
{
    return physics_loop_period_;
}

void ASimModeWorldBase::setPhysicsLoopPeriod(long long period)
{
    physics_loop_period_ = period;
}

std::unique_ptr<ASimModeWorldBase::PhysicsEngineBase> ASimModeWorldBase::createPhysicsEngine()
{
    std::unique_ptr<PhysicsEngineBase> physics_engine;
    std::string physics_engine_name = getSettings().physics_engine_name;

    if (physics_engine_name == "")
        physics_engine.reset();
    else if (physics_engine_name == "FastPhysicsEngine") {
        msr::airlib::Settings fast_phys_settings;
        if (msr::airlib::Settings::singleton().getChild("FastPhysicsEngine", fast_phys_settings)) {
            physics_engine.reset(new msr::airlib::FastPhysicsEngine(
                fast_phys_settings.getBool("EnableGroundLock", true)));
        }
        else {
            physics_engine.reset(new msr::airlib::FastPhysicsEngine());
        }

        physics_engine->setWind(getSettings().wind);
        physics_engine->setExtForce(getSettings().ext_force);
    }
    else if (physics_engine_name == "ExternalPhysicsEngine") {
        physics_engine.reset(new msr::airlib::ExternalPhysicsEngine());
    }
    else {
        physics_engine.reset();
        UAirBlueprintLib::LogMessageString("Unrecognized physics engine name: ",
            physics_engine_name, LogDebugLevel::Failure);
    }

    return physics_engine;
}

bool ASimModeWorldBase::isPaused() const
{
    return physics_world_->isPaused();
}

void ASimModeWorldBase::pause(bool is_paused)
{
    physics_world_->pause(is_paused);
    ASimModeBase::pause(is_paused);
}

void ASimModeWorldBase::continueForTime(double seconds)
{
    int64 start_frame_number = UKismetSystemLibrary::GetFrameCount();
    if (physics_world_->isPaused()) {
        physics_world_->pause(false);
        UGameplayStatics::SetGamePaused(this->GetWorld(), false);
    }

    physics_world_->continueForTime(seconds);
    while (!physics_world_->isPaused()) { continue; }
    while (start_frame_number == UKismetSystemLibrary::GetFrameCount()) { continue; }

    UGameplayStatics::SetGamePaused(this->GetWorld(), true);
}

void ASimModeWorldBase::continueForFrames(uint32_t frames)
{
    if (physics_world_->isPaused()) {
        physics_world_->pause(false);
        UGameplayStatics::SetGamePaused(this->GetWorld(), false);
    }

    physics_world_->setFrameNumber((uint32_t)GFrameNumber);
    physics_world_->continueForFrames(frames);

    while (!physics_world_->isPaused()) {
        physics_world_->setFrameNumber((uint32_t)GFrameNumber);
    }

    UGameplayStatics::SetGamePaused(this->GetWorld(), true);
}

void ASimModeWorldBase::setWind(const msr::airlib::Vector3r& wind) const
{
    physics_engine_->setWind(wind);
}

void ASimModeWorldBase::setExtForce(const msr::airlib::Vector3r& ext_force) const
{
    physics_engine_->setExtForce(ext_force);
}

void ASimModeWorldBase::updateDebugReport(msr::airlib::StateReporterWrapper& debug_reporter)
{
    unused(debug_reporter);
}

void ASimModeWorldBase::Tick(float DeltaSeconds)
{
    { // lock court
        physics_world_->lock();

        physics_world_->enableStateReport(EnableReport);
        physics_world_->updateStateReport();
        for (auto& api : getApiProvider()->getVehicleSimApis()) {
            if (isAirLibManagedVehicle(api)) {
                api->updateRenderedState(DeltaSeconds);
            }
        }

        physics_world_->unlock();
    }

    // Rendering update hors lock, seulement multirotor
    for (auto& api : getApiProvider()->getVehicleSimApis()) {
        if (isAirLibManagedVehicle(api)) {
            api->updateRendering(DeltaSeconds);
        }
    }

    Super::Tick(DeltaSeconds);
}

void ASimModeWorldBase::reset()
{
    UAirBlueprintLib::RunCommandOnGameThread([this]() {
        physics_world_->reset();
    }, true);
}

std::string ASimModeWorldBase::getDebugReport()
{
    return physics_world_->getDebugReport();
}
