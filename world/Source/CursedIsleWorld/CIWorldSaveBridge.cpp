// CIWorldSaveBridge.cpp
#include "CIWorldSaveBridge.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogCIWorldSave, Log, All);

FCIWorldSaveBridge::FCIWorldSaveBridge()
{
    // 允许用环境变量指到别处；默认取工程目录下 ../data/users.json
    const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("CI_SAVE_PATH"));
    if (!Env.IsEmpty())
        FilePath = Env;
    else
        FilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("data"), TEXT("users.json"));
    FPaths::CollapseRelativeDirectories(FilePath);
    Root = MakeShared<FJsonObject>();
}

bool FCIWorldSaveBridge::Load()
{
    FScopeLock Lock(&Mutex);
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *FilePath))
    {
        UE_LOG(LogCIWorldSave, Warning, TEXT("[Save] 存档不存在，新建空表: %s"), *FilePath);
        Root = MakeShared<FJsonObject>();
        Root->SetObjectField(TEXT("users"), MakeShared<FJsonObject>());
        return true;
    }
    TSharedPtr<FJsonObject> Parsed;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
    {
        UE_LOG(LogCIWorldSave, Error, TEXT("[Save] JSON 解析失败，拒绝覆盖: %s"), *FilePath);
        return false;
    }
    Root = Parsed;
    if (!Root->HasField(TEXT("users")))
        Root->SetObjectField(TEXT("users"), MakeShared<FJsonObject>());
    return true;
}

bool FCIWorldSaveBridge::Save() const
{
    FScopeLock Lock(&Mutex);
    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) return false;

    // 原子写，避免 REST 后端同时读到半截文件
    const FString Tmp = FilePath + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(Out, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        return false;
    IFileManager::Get().Move(*FilePath, *Tmp, true, true);
    return true;
}

static TSharedPtr<FJsonObject> UsersObj(const TSharedPtr<FJsonObject>& Root)
{
    const TSharedPtr<FJsonObject>* Found = nullptr;
    if (Root->TryGetObjectField(TEXT("users"), Found) && Found && Found->IsValid())
        return *Found;
    TSharedPtr<FJsonObject> New = MakeShared<FJsonObject>();
    Root->SetObjectField(TEXT("users"), New);
    return New;
}

bool FCIWorldSaveBridge::FindByUuid(const FString& Uuid, FCIWorldPlayerSnapshot& Out) const
{
    FScopeLock Lock(&Mutex);
    if (Uuid.IsEmpty()) return false;

    TSharedPtr<FJsonObject> Users = UsersObj(Root);
    for (const auto& Pair : Users->Values)
    {
        const TSharedPtr<FJsonObject>* U = nullptr;
        if (!Pair.Value->TryGetObject(U) || !U || !U->IsValid()) continue;
        FString Id;
        if (!(*U)->TryGetStringField(TEXT("uuid"), Id) || Id != Uuid) continue;

        (*U)->TryGetStringField(TEXT("login"), Out.Login);
        (*U)->TryGetStringField(TEXT("selected_dino_id"), Out.DinoId);
        (*U)->TryGetStringField(TEXT("selected_avatar_id"), Out.AvatarId);
        Out.Uuid = Uuid;
        (*U)->TryGetNumberField(TEXT("level"), Out.Level);
        double Xp = 0.0, Hp = 100.0, Hu = 100.0, St = 100.0;
        if ((*U)->TryGetNumberField(TEXT("world_experience"), Xp)) Out.Experience = (float)Xp;
        if ((*U)->TryGetNumberField(TEXT("world_health"), Hp))    Out.Health = (float)Hp;
        if ((*U)->TryGetNumberField(TEXT("world_hunger"), Hu))    Out.Hunger = (float)Hu;
        if ((*U)->TryGetNumberField(TEXT("world_stamina"), St))   Out.Stamina = (float)St;
        bool bAlive = true;
        if ((*U)->TryGetBoolField(TEXT("world_alive"), bAlive))   Out.bAlive = bAlive;

        const TArray<TSharedPtr<FJsonValue>>* Pos = nullptr;
        if ((*U)->TryGetArrayField(TEXT("world_pos"), Pos) && Pos && Pos->Num() >= 3)
        {
            Out.Position = FVector((*Pos)[0]->AsNumber(), (*Pos)[1]->AsNumber(), (*Pos)[2]->AsNumber());
        }
        if (Out.DinoId.IsEmpty())   Out.DinoId = TEXT("troo");
        if (Out.AvatarId.IsEmpty()) Out.AvatarId = TEXT("galli");
        return true;
    }
    return false;
}

void FCIWorldSaveBridge::Upsert(const FCIWorldPlayerSnapshot& S)
{
    FScopeLock Lock(&Mutex);
    if (S.Uuid.IsEmpty()) return;

    TSharedPtr<FJsonObject> Users = UsersObj(Root);
    const FString Key = S.Login.IsEmpty() ? S.Uuid : (S.Login + TEXT("|world"));

    TSharedPtr<FJsonObject> U;
    const TSharedPtr<FJsonObject>* Existing = nullptr;
    // 先按 uuid 找已有条目，保留 REST 侧 token/货币；找不到才新建
    for (const auto& Pair : Users->Values)
    {
        const TSharedPtr<FJsonObject>* Cand = nullptr;
        if (!Pair.Value->TryGetObject(Cand) || !Cand || !Cand->IsValid()) continue;
        FString Id;
        if ((*Cand)->TryGetStringField(TEXT("uuid"), Id) && Id == S.Uuid)
        {
            U = *Cand;
            break;
        }
    }
    if (!U.IsValid())
    {
        U = MakeShared<FJsonObject>();
        U->SetStringField(TEXT("uuid"), S.Uuid);
        U->SetStringField(TEXT("login"), S.Login);
        U->SetStringField(TEXT("device_number"), TEXT("ds"));
        U->SetStringField(TEXT("user_auth_token"), TEXT("issued-by-ds"));
        U->SetNumberField(TEXT("currency_soft"), 999999);
        U->SetNumberField(TEXT("currency_hard"), 999999);
        U->SetNumberField(TEXT("created"), FPlatformTime::Seconds());
        Users->SetObjectField(Key, U);
    }

    // ★ 只更新世界层字段，绝不覆盖 REST 侧的身份/货币字段
    U->SetNumberField(TEXT("level"), S.Level);
    U->SetNumberField(TEXT("world_experience"), S.Experience);
    U->SetNumberField(TEXT("world_health"), S.Health);
    U->SetNumberField(TEXT("world_hunger"), S.Hunger);
    U->SetNumberField(TEXT("world_stamina"), S.Stamina);
    U->SetBoolField(TEXT("world_alive"), S.bAlive);
    U->SetStringField(TEXT("selected_dino_id"), S.DinoId);
    U->SetStringField(TEXT("selected_avatar_id"), S.AvatarId);
    TArray<TSharedPtr<FJsonValue>> Pos;
    Pos.Add(MakeShared<FJsonValueNumber>(S.Position.X));
    Pos.Add(MakeShared<FJsonValueNumber>(S.Position.Y));
    Pos.Add(MakeShared<FJsonValueNumber>(S.Position.Z));
    U->SetArrayField(TEXT("world_pos"), Pos);
    U->SetNumberField(TEXT("world_last_saved"), FPlatformTime::Seconds());
    U->SetNumberField(TEXT("last_seen"), FPlatformTime::Seconds());
}
