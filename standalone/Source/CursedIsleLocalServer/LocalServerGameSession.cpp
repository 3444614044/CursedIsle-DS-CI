// LocalServerGameSession.cpp
#include "LocalServerGameSession.h"

DEFINE_LOG_CATEGORY_STATIC(LogLocalDSSession, Log, All);

FString ALocalServerGameSession::ApproveLogin(const FString& Options)
{
    UE_LOG(LogLocalDSSession, Warning, TEXT("[LocalDS] GameSession::ApproveLogin 放行  Options=%s"), *Options);
    // 空串 = 允许。真实服务端应在这里校验 REST 后端签发的 token。
    return FString();
}
