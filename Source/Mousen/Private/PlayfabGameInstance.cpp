// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayFabGameInstance.h"
#include "PlayfabGSDK.h"
#include "GSDKUtils.h"

DEFINE_LOG_CATEGORY(LogPlayFabGSDKGameInstance);

void UPlayfabGameInstance::Init()
{
    if (IsDedicatedServerInstance() == true)
    {
        FOnGSDKShutdown_Dyn OnGsdkShutdown;
        OnGsdkShutdown.BindDynamic(this, &UPlayfabGameInstance::OnGSDKShutdown);
        FOnGSDKHealthCheck_Dyn OnGsdkHealthCheck;
        OnGsdkHealthCheck.BindDynamic(this, &UPlayfabGameInstance::OnGSDKHealthCheck);
        FOnGSDKServerActive_Dyn OnGSDKServerActive;
        OnGSDKServerActive.BindDynamic(this, &UPlayfabGameInstance::OnGSDKServerActive);
        FOnGSDKReadyForPlayers_Dyn OnGSDKReadyForPlayers;
        OnGSDKReadyForPlayers.BindDynamic(this, &UPlayfabGameInstance::OnGSDKReadyForPlayers);

        UGSDKUtils::RegisterGSDKShutdownDelegate(OnGsdkShutdown);
        UGSDKUtils::RegisterGSDKHealthCheckDelegate(OnGsdkHealthCheck);
        UGSDKUtils::RegisterGSDKServerActiveDelegate(OnGSDKServerActive);
        UGSDKUtils::RegisterGSDKReadyForPlayers(OnGSDKReadyForPlayers);
    }
#if UE_SERVER
    UGSDKUtils::SetDefaultServerHostPort();
#endif
}

void UPlayfabGameInstance::OnStart()
{
    UE_LOG(LogPlayFabGSDKGameInstance, Warning, TEXT("Reached onStart!"));
    UGSDKUtils::ReadyForPlayers();
}

void UPlayfabGameInstance::OnGSDKShutdown()
{
    UE_LOG(LogPlayFabGSDKGameInstance, Warning, TEXT("Shutdown!"));
    FPlatformMisc::RequestExit(false);
}

bool UPlayfabGameInstance::OnGSDKHealthCheck()
{
    // Uncomment the next line if you want your server to log something at every heartbeat for sanity check.
    /* UE_LOG(LogPlayFabGSDKGameInstance, Warning, TEXT("Healthy!")); */
    return true;
}

void UPlayfabGameInstance::OnGSDKServerActive()
{
    /**
 * Server is transitioning to an active state.
 * Optional: Add in the implementation any code that is needed for the game server when
 * this transition occurs.
 */
    UE_LOG(LogPlayFabGSDKGameInstance, Warning, TEXT("Active!"));
}

void UPlayfabGameInstance::OnGSDKReadyForPlayers()
{
    /**
 * Server is transitioning to a StandBy state. Game initialization is complete and the game
 * is ready to accept players.
 * Optional: Add in the implementation any code that is needed for the game server before
 * initialization completes.
 */
    UE_LOG(LogPlayFabGSDKGameInstance, Warning, TEXT("Finished Initialization - Moving to StandBy!"));
}
