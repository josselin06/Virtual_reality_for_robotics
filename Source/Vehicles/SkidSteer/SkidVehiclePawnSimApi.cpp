// Developed by Cosys-Lab, University of Antwerp

#include "SkidVehiclePawnSimApi.h"
#include "AirBlueprintLib.h"
#include "UnrealSensors/UnrealSensorFactory.h"
#include "SkidVehiclePawnApi.h"
#include <exception>
#include <cmath>

using namespace msr::airlib;

SkidVehiclePawnSimApi::SkidVehiclePawnSimApi(const Params& params,
                                             const msr::airlib::CarApiBase::CarControls& keyboard_controls)
    : PawnSimApi(params), keyboard_controls_(keyboard_controls)
{
}

void SkidVehiclePawnSimApi::initialize()
{
    PawnSimApi::initialize();

    std::shared_ptr<UnrealSensorFactory> sensor_factory = std::make_shared<UnrealSensorFactory>(getPawn(), &getNedTransform());

    vehicle_api_ = CarApiFactory::createApi(getVehicleSetting(),
        sensor_factory,
        *getGroundTruthKinematics(),
        *getGroundTruthEnvironment());

    pawn_api_ = std::unique_ptr<SkidVehiclePawnApi>(
        new SkidVehiclePawnApi(static_cast<ASkidVehiclePawn*>(getPawn()), getGroundTruthKinematics(), vehicle_api_.get())
    );

    joystick_controls_ = msr::airlib::CarApiBase::CarControls();
}

std::string SkidVehiclePawnSimApi::getRecordFileLine(bool is_header_line) const
{
    std::string common_line = PawnSimApi::getRecordFileLine(is_header_line);
    if (is_header_line) {
        return common_line +
            "Throttle\tSteering\tBrake\tGear\tHandbrake\tRPM\tSpeed\t";
    }

    const auto& state = pawn_api_->getCarState();

    std::ostringstream ss;
    ss << common_line;
    ss << current_controls_.throttle << "\t" << current_controls_.steering << "\t" << current_controls_.brake << "\t";
    ss << state.gear << "\t" << state.handbrake << "\t" << state.rpm << "\t" << state.speed << "\t";

    return ss.str();
}

// these are called on render ticks
void SkidVehiclePawnSimApi::updateRenderedState(float dt)
{
    PawnSimApi::updateRenderedState(dt);

    vehicle_api_->getStatusMessages(vehicle_api_messages_);

    if (getRemoteControlID() >= 0)
        vehicle_api_->setRCData(getRCData());
}

void SkidVehiclePawnSimApi::updateRendering(float dt)
{
    PawnSimApi::updateRendering(dt);

    updateCarControls();

    for (auto i = 0; i < vehicle_api_messages_.size(); ++i) {
        UAirBlueprintLib::LogMessage(FString(vehicle_api_messages_[i].c_str()), TEXT(""), LogDebugLevel::Success, 30);
    }

    try {
        vehicle_api_->sendTelemetry(dt);
    }
    catch (std::exception& e) {
        UAirBlueprintLib::LogMessage(FString(e.what()), TEXT(""), LogDebugLevel::Failure, 30);
    }
}

static bool hasNonZeroApiControls(const msr::airlib::CarApiBase::CarControls& c)
{
    const float eps = 1e-4f;
    return (std::fabs(c.throttle) > eps) ||
           (std::fabs(c.steering) > eps) ||
           (std::fabs(c.brake) > eps) ||
           (c.handbrake != 0) ||
           (c.is_manual_gear);
}

void SkidVehiclePawnSimApi::updateCarControls()
{
    auto rc_data = getRCData();

    if (rc_data.is_initialized) {
        if (!rc_data.is_valid) {
            UAirBlueprintLib::LogMessageString("Control Mode: ", "[INVALID] Wheel/Joystick", LogDebugLevel::Informational);
            return;
        }
        UAirBlueprintLib::LogMessageString("Control Mode: ", "Wheel/Joystick", LogDebugLevel::Informational);

        if (rc_data.vendor_id == "VID_044F") {
            joystick_controls_.steering = rc_data.yaw;
            joystick_controls_.throttle = (-rc_data.right_z + 1) / 2;
            joystick_controls_.brake = rc_data.throttle;

            auto car_state = vehicle_api_->getCarState();
            float rumble_strength = 0.66f + (car_state.rpm / car_state.maxrpm) / 3.0f;
            float auto_center = (1.0f - 1.0f / (std::abs(car_state.speed / 120.0f) + 1.0f)) * (rc_data.yaw / 3.0f);
            setRCForceFeedback(rumble_strength, auto_center);
        }
        else {
            joystick_controls_.steering = (rc_data.throttle * 2 - 1) * 1.25f;
            joystick_controls_.throttle = (-rc_data.roll + 1) / 2;
            joystick_controls_.brake = -rc_data.right_z + 1;
        }

        joystick_controls_.handbrake = (rc_data.getSwitch(5)) | (rc_data.getSwitch(6)) ? 1 : 0;

        if ((rc_data.getSwitch(8)) | (rc_data.getSwitch(1))) {
            joystick_controls_.manual_gear = -1;
            joystick_controls_.is_manual_gear = true;
            joystick_controls_.gear_immediate = true;
        }
        else if ((rc_data.getSwitch(9)) | (rc_data.getSwitch(0))) {
            joystick_controls_.manual_gear = 0;
            joystick_controls_.is_manual_gear = false;
            joystick_controls_.gear_immediate = true;
        }

        current_controls_ = joystick_controls_;
    }
    else {
        UAirBlueprintLib::LogMessageString("Control Mode: ", "Keyboard", LogDebugLevel::Informational);
        current_controls_ = keyboard_controls_;
    }

    const auto api_controls = vehicle_api_->getCarControls();
    const bool api_wants_control = vehicle_api_->isApiControlEnabled() || hasNonZeroApiControls(api_controls);

    if (api_wants_control) {
        UAirBlueprintLib::LogMessageString("Control Mode: ", "API", LogDebugLevel::Informational);
        current_controls_ = api_controls;
        pawn_api_->updateMovement(current_controls_);
    }
    else {
        vehicle_api_->setCarControls(current_controls_);
        pawn_api_->updateMovement(current_controls_);
    }

    UAirBlueprintLib::LogMessageString("Accel: ", std::to_string(current_controls_.throttle), LogDebugLevel::Informational);
    UAirBlueprintLib::LogMessageString("Break: ", std::to_string(current_controls_.brake), LogDebugLevel::Informational);
    UAirBlueprintLib::LogMessageString("Steering: ", std::to_string(current_controls_.steering), LogDebugLevel::Informational);
    UAirBlueprintLib::LogMessageString("Handbrake: ", std::to_string(current_controls_.handbrake), LogDebugLevel::Informational);
    UAirBlueprintLib::LogMessageString("Target Gear: ", std::to_string(current_controls_.manual_gear), LogDebugLevel::Informational);
}

void SkidVehiclePawnSimApi::resetImplementation()
{
    PawnSimApi::resetImplementation();
    pawn_api_->reset();
}

// physics tick
void SkidVehiclePawnSimApi::update(float delta)
{
    pawn_api_->update(delta);
    PawnSimApi::update(delta);
}

void SkidVehiclePawnSimApi::reportState(StateReporter& reporter)
{
    PawnSimApi::reportState(reporter);
    vehicle_api_->reportState(reporter);
}

