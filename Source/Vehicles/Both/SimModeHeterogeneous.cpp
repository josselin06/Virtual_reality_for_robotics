#include "SimModeHeterogeneous.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "AirBlueprintLib.h"
#include "common/AirSimSettings.hpp"


#include "vehicles/multirotor/api/MultirotorApiBase.hpp"
#include "Vehicles/Multirotor/MultirotorPawnSimApi.h"

#include "Vehicles/SkidSteer/SkidVehiclePawnSimApi.h"

#include "Vehicles/Multirotor/FlyingPawn.h"
#include "Vehicles/SkidSteer/SkidVehiclePawn.h"

#include "Vehicles/Both/HeterogeneousApiServer.h"

#include <memory>

#if __has_include("Vehicles/Car/CarPawn.h") && __has_include("Vehicles/Car/CarPawnSimApi.h")
    #define AIRSIM_HET_HAS_CAR 1
    #include "Vehicles/Car/CarPawn.h"
    #include "Vehicles/Car/CarPawnSimApi.h"
#else
    #define AIRSIM_HET_HAS_CAR 0
#endif

// Helper: call initializeForBeginPlay(...) with or without engine_sound depending on what exists
template <typename PawnT, typename SoundT>
static auto callInitializeForBeginPlay(PawnT* pawn, const SoundT& sound, int)
    -> decltype(pawn->initializeForBeginPlay(sound), void())
{
    pawn->initializeForBeginPlay(sound);
}

template <typename PawnT, typename SoundT>
static auto callInitializeForBeginPlay(PawnT* pawn, const SoundT&, long)
    -> decltype(pawn->initializeForBeginPlay(), void())
{
    pawn->initializeForBeginPlay();
}

void ASimModeWorldHeterogeneous::BeginPlay()
{
    Super::BeginPlay();
    UAirBlueprintLib::LogMessageString("HETERO: BeginPlay done (no manual initializeForPlay)", "", LogDebugLevel::Informational);
}

void ASimModeWorldHeterogeneous::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    stopAsyncUpdator();
    Super::EndPlay(EndPlayReason);
}

std::unique_ptr<msr::airlib::ApiServerBase> ASimModeWorldHeterogeneous::createApiServer() const
{
    const auto& settings = getSettings();

    // multirotor port = ApiServerPort (default 41451)
    const uint16_t multirotor_port = static_cast<uint16_t>(settings.api_port);

    // car port = ApiServerPort + 1 (default 41452)
    const uint16_t car_port = static_cast<uint16_t>(settings.api_port + 1);

    return std::make_unique<msr::airlib::HeterogeneousApiServer>(
        getApiProvider(),
        settings.api_server_address,
        multirotor_port,
        car_port
    );
}

void ASimModeWorldHeterogeneous::getExistingVehiclePawns(TArray<AActor*>& pawns) const
{
    UAirBlueprintLib::FindAllActor<AFlyingPawn>(this, pawns);
    UAirBlueprintLib::FindAllActor<ASkidVehiclePawn>(this, pawns);

#if AIRSIM_HET_HAS_CAR
    UAirBlueprintLib::FindAllActor<ACarPawn>(this, pawns);
#endif
}

bool ASimModeWorldHeterogeneous::isVehicleTypeSupported(const std::string& vehicle_type) const
{
    return (
        // multirotor
        vehicle_type == AirSimSettings::kVehicleTypeSimpleFlight ||
        vehicle_type == AirSimSettings::kVehicleTypePX4 ||
        vehicle_type == AirSimSettings::kVehicleTypeArduCopterSolo ||
        vehicle_type == AirSimSettings::kVehicleTypeArduCopter ||

        // skid
        vehicle_type == AirSimSettings::kVehicleTypeCPHusky ||
        vehicle_type == AirSimSettings::kVehicleTypePioneer ||

        // car (supported if present in your settings; actual control requires AIRSIM_HET_HAS_CAR==1)
        vehicle_type == AirSimSettings::kVehicleTypePhysXCar ||
        vehicle_type == AirSimSettings::kVehicleTypeBoxCar
    );
}

std::string ASimModeWorldHeterogeneous::getVehiclePawnPathName(const AirSimSettings::VehicleSetting& vehicle_setting) const
{
    std::string pawn_path = vehicle_setting.pawn_path;

    if (pawn_path == "") {
        // Car
        if (vehicle_setting.vehicle_type == AirSimSettings::kVehicleTypePhysXCar ||
            vehicle_setting.vehicle_type == AirSimSettings::kVehicleTypeBoxCar) {
            pawn_path = "DefaultCar";
        }
        // Skid
        else if (vehicle_setting.vehicle_type == AirSimSettings::kVehicleTypePioneer ||
                 vehicle_setting.vehicle_type == "pioneer") {
            pawn_path = "Pioneer";
        }
        else if (vehicle_setting.vehicle_type == AirSimSettings::kVehicleTypeCPHusky) {
            pawn_path = "DefaultSkidVehicle";
        }
        // Multirotor
        else {
            pawn_path = "DefaultQuadrotor";
        }
    }

    return pawn_path;
}

PawnEvents* ASimModeWorldHeterogeneous::getVehiclePawnEvents(APawn* pawn) const
{
    if (auto* flying = Cast<AFlyingPawn>(pawn)) {
        return flying->getPawnEvents();
    }
    if (auto* skid = Cast<ASkidVehiclePawn>(pawn)) {
        return skid->getPawnEvents();
    }

#if AIRSIM_HET_HAS_CAR
    if (auto* car = Cast<ACarPawn>(pawn)) {
        return car->getPawnEvents();
    }
#endif

    return nullptr;
}

const common_utils::UniqueValueMap<std::string, APIPCamera*> ASimModeWorldHeterogeneous::getVehiclePawnCameras(APawn* pawn) const
{
    if (auto* flying = Cast<AFlyingPawn>(pawn)) {
        return flying->getCameras();
    }
    if (auto* skid = Cast<ASkidVehiclePawn>(pawn)) {
        return skid->getCameras();
    }

#if AIRSIM_HET_HAS_CAR
    if (auto* car = Cast<ACarPawn>(pawn)) {
        return car->getCameras();
    }
#endif

    return common_utils::UniqueValueMap<std::string, APIPCamera*>();
}

void ASimModeWorldHeterogeneous::initializeVehiclePawn(APawn* pawn)
{
    if (auto* flying = Cast<AFlyingPawn>(pawn)) {
        flying->initializeForBeginPlay();
        return;
    }
    if (auto* skid = Cast<ASkidVehiclePawn>(pawn)) {
        skid->initializeForBeginPlay(getSettings().engine_sound);
        return;
    }

#if AIRSIM_HET_HAS_CAR
    if (auto* car = Cast<ACarPawn>(pawn)) {
        // Auto-selects initializeForBeginPlay(engine_sound) OR initializeForBeginPlay()
        callInitializeForBeginPlay(car, getSettings().engine_sound, 0);
        return;
    }
#endif
}

std::unique_ptr<PawnSimApi> ASimModeWorldHeterogeneous::createVehicleSimApi(const PawnSimApi::Params& pawn_sim_api_params) const
{
    APawn* pawn = pawn_sim_api_params.pawn;

    if (auto* flying = Cast<AFlyingPawn>(pawn)) {
        auto sim_api = std::unique_ptr<PawnSimApi>(new MultirotorPawnSimApi(pawn_sim_api_params));
        sim_api->initialize();
        return sim_api;
    }

    if (auto* skid = Cast<ASkidVehiclePawn>(pawn)) {
        auto sim_api = std::unique_ptr<PawnSimApi>(
            new SkidVehiclePawnSimApi(pawn_sim_api_params, skid->getKeyBoardControls())
        );
        sim_api->initialize();
        return sim_api;
    }

#if AIRSIM_HET_HAS_CAR
    if (auto* car_pawn = Cast<ACarPawn>(pawn)) {
        auto sim_api = std::unique_ptr<PawnSimApi>(
            new CarPawnSimApi(pawn_sim_api_params, car_pawn->getKeyBoardControls())
        );
        sim_api->initialize();
        sim_api->reset();
        return sim_api;
    }
#endif

    return nullptr;
}

msr::airlib::VehicleApiBase* ASimModeWorldHeterogeneous::getVehicleApi(
    const PawnSimApi::Params& pawn_sim_api_params,
    const PawnSimApi* sim_api
) const
{
    APawn* pawn = pawn_sim_api_params.pawn;

    if (Cast<AFlyingPawn>(pawn)) {
        auto* multi = static_cast<const MultirotorPawnSimApi*>(sim_api);
        return multi ? multi->getVehicleApi() : nullptr;
    }

    if (Cast<ASkidVehiclePawn>(pawn)) {
        auto* skid = static_cast<const SkidVehiclePawnSimApi*>(sim_api);
        return skid ? skid->getVehicleApi() : nullptr;
    }

#if AIRSIM_HET_HAS_CAR
    if (Cast<ACarPawn>(pawn)) {
        auto* car_api = static_cast<const CarPawnSimApi*>(sim_api);
        return car_api ? car_api->getVehicleApi() : nullptr;
    }
#endif

    return nullptr;
}
