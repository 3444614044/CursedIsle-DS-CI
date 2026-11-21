// LocalGameMode.cpp
#include "LocalGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"

ALocalGameMode::ALocalGameMode()
{
    bUseSeamlessTravel = false;
    bAllowAnonymousLocalLogin = true;
}

void ALocalGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    UE_LOG(LogTemp, Log, TEXT("[LocalDS] InitGame Map=%s Options=%s"), *MapName, *Options);
}

void ALocalGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    UE_LOG(LogTemp, Log, TEXT("[LocalDS] Player joined: %s"),
           NewPlayer ? *NewPlayer->GetName() : TEXT("null"));
}

void ALocalGameMode::Logout(AController* Exiting)
{
    UE_LOG(LogTemp, Log, TEXT("[LocalDS] Player left"));
    Super::Logout(Exiting);
}
