#include "BsGameFunctionLibrary.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHIGPUReadback.h"

#define BS_SCREEN_WIDTH 1920
#define BS_SCREEN_HEIGHT 1080
#define BS_INIT_LAND_HEIGHT 128
#define BS_ENEMY_MAX 1024
#define BS_BULLET_MAX 65536

#define BS_ADDRESS_ENEMY_CS_ARG			(0)
#define BS_ADDRESS_ENEMY_BULLET_CS_ARG	(BS_ADDRESS_ENEMY_CS_ARG + 3)
#define BS_ADDRESS_RE_BULLET_CS_ARG		(BS_ADDRESS_ENEMY_BULLET_CS_ARG + 3)
#define BS_ADDRESS_BOMB_CS_ARG			(BS_ADDRESS_RE_BULLET_CS_ARG + 3)
#define BS_ADDRESS_ENEMY_VS_ARG			(BS_ADDRESS_BOMB_CS_ARG + 3)
#define BS_ADDRESS_ENEMY_BULLET_VS_ARG	(BS_ADDRESS_ENEMY_VS_ARG + 4)
#define BS_ADDRESS_RE_BULLET_VS_ARG		(BS_ADDRESS_ENEMY_BULLET_VS_ARG + 4)
#define BS_ADDRESS_BOMB_VS_ARG			(BS_ADDRESS_RE_BULLET_VS_ARG + 4)

#define BS_SOUND_TITLE					0
#define BS_SOUND_GAMEOVER				1
#define BS_SOUND_SELECT					2
#define BS_SOUND_SHOT					3
#define BS_SOUND_BOMB					4
#define BS_SOUND_REFLECT				5
#define BS_SOUND_PUSH					6
#define BS_SOUND_MAX					256


UBsGameFunctionLibrary::UBsGameFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }


struct FTargetData
{
	FVector2f UV;
	float Radius;
};

struct FGameState
{
	int32 CurrentState;
	int32 PlayerDead;
	int32 Score;
	int32 HiScore;
	int32 GameFrame;
	int32 EnemyInterval;
	int tmp[2];
};

struct FPlayerState
{
	FVector2f Position;
	FVector2f BarrierPosition;
	FVector2f PrevBarrierPosition;
	float FallSpeed;
	float BarrierAngle;
	float PrevBarrierAngle;
	int BarrierShootTime;
	int BarrierCoolTime;
	int tmp;
};

struct FEnemyState
{
	FVector2f Position;
	FVector2f Target;
	float ShootAngle;
	int ShootInterval;
	int tmp;
};

struct FBulletState
{
	FVector2f Position;
	FVector2f Velocity;
};

struct FBombState
{
	FVector2f Position;
	int Count;
	int tmp;
};

class FInitGameCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInitGameCS);
	SHADER_USE_PARAMETER_STRUCT(FInitGameCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FPlayerState>, rwPlayerState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint>, rwStage)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount0)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount1)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwRandomCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FStartGameCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FStartGameCS);
	SHADER_USE_PARAMETER_STRUCT(FStartGameCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FPlayerState>, rwPlayerState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint>, rwStage)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount0)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount1)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwSound)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
IMPLEMENT_SHADER_TYPE(,FInitGameCS,TEXT("/Plugin/BsGame/Private/BsInitialize.usf"),TEXT("InitGameCS"),SF_Compute)
IMPLEMENT_SHADER_TYPE(,FStartGameCS,TEXT("/Plugin/BsGame/Private/BsInitialize.usf"),TEXT("StartGameCS"),SF_Compute)


class FTickStartCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTickStartCS);
	SHADER_USE_PARAMETER_STRUCT(FTickStartCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, rCount)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint>, rwArgument)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTickEndCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTickEndCS);
	SHADER_USE_PARAMETER_STRUCT(FTickEndCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint>, rwArgument)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTickPlayerCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTickPlayerCS);
	SHADER_USE_PARAMETER_STRUCT(FTickPlayerCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FPlayerState>, rwPlayerState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint>, rwStage)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwSound)
		SHADER_PARAMETER(float, LeftRightMove)
		SHADER_PARAMETER(FVector2f, MousePos)
		SHADER_PARAMETER(int, bPressAttack)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTickEnemyCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTickEnemyCS);
	SHADER_USE_PARAMETER_STRUCT(FTickEnemyCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_SRV(StructuredBuffer<FEnemyState>, rEnemys)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FEnemyState>, rwEnemys)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FBulletState>, rwBullets)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FBombState>, rwBombs)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, rRandom)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwRandomCount)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, rCount)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwSound)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTickAddEnemyCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTickAddEnemyCS);
	SHADER_USE_PARAMETER_STRUCT(FTickAddEnemyCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FEnemyState>, rwEnemys)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, rRandom)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwRandomCount)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTickEnemyBulletCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTickEnemyBulletCS);
	SHADER_USE_PARAMETER_STRUCT(FTickEnemyBulletCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FPlayerState>, rwPlayerState)
		SHADER_PARAMETER_SRV(StructuredBuffer<FBulletState>, rBullets)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FBulletState>, rwBullets)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FBulletState>, rwReBullets)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FBombState>, rwBombs)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint>, rwStage)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, rRandom)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwRandomCount)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, rCount)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwSound)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTickReBulletCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTickReBulletCS);
	SHADER_USE_PARAMETER_STRUCT(FTickReBulletCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FEnemyState>, rwEnemys)
		SHADER_PARAMETER_SRV(StructuredBuffer<FBulletState>, rReBullets)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FBulletState>, rwReBullets)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint>, rwStage)
		SHADER_PARAMETER_SRV(StructuredBuffer<float>, rRandom)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwRandomCount)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, rCount)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTickBombCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTickBombCS);
	SHADER_USE_PARAMETER_STRUCT(FTickBombCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FGameState>, rwGameState)
		SHADER_PARAMETER_SRV(StructuredBuffer<FBombState>, rBombs)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<FBombState>, rwBombs)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, rCount)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, rwCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
IMPLEMENT_SHADER_TYPE(,FTickStartCS,TEXT("/Plugin/BsGame/Private/BsTick.usf"),TEXT("TickStartCS"),SF_Compute)
IMPLEMENT_SHADER_TYPE(,FTickEndCS,TEXT("/Plugin/BsGame/Private/BsTick.usf"),TEXT("TickEndCS"),SF_Compute)
IMPLEMENT_SHADER_TYPE(,FTickPlayerCS,TEXT("/Plugin/BsGame/Private/BsTick.usf"),TEXT("TickPlayerCS"),SF_Compute)
IMPLEMENT_SHADER_TYPE(,FTickEnemyCS,TEXT("/Plugin/BsGame/Private/BsTick.usf"),TEXT("TickEnemyCS"),SF_Compute)
IMPLEMENT_SHADER_TYPE(,FTickAddEnemyCS,TEXT("/Plugin/BsGame/Private/BsTick.usf"),TEXT("TickAddEnemyCS"),SF_Compute)
IMPLEMENT_SHADER_TYPE(,FTickEnemyBulletCS,TEXT("/Plugin/BsGame/Private/BsTick.usf"),TEXT("TickEnemyBulletCS"),SF_Compute)
IMPLEMENT_SHADER_TYPE(,FTickReBulletCS,TEXT("/Plugin/BsGame/Private/BsTick.usf"),TEXT("TickReBulletCS"),SF_Compute)
IMPLEMENT_SHADER_TYPE(,FTickBombCS,TEXT("/Plugin/BsGame/Private/BsTick.usf"),TEXT("TickBombCS"),SF_Compute)


class FFullScreenVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFullScreenVS);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	/** Default constructor. */
	FFullScreenVS() {}

	/** Initialization constructor. */
	FFullScreenVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};
class FRenderStagePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRenderStagePS);
	SHADER_USE_PARAMETER_STRUCT(FRenderStagePS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, rStage)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FPlayerSpriteVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPlayerSpriteVS);
	SHADER_USE_PARAMETER_STRUCT(FPlayerSpriteVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGameState>, rGameState)
		SHADER_PARAMETER_SRV(StructuredBuffer<FPlayerState>, rPlayerState)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FBarrierSpriteVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBarrierSpriteVS);
	SHADER_USE_PARAMETER_STRUCT(FBarrierSpriteVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGameState>, rGameState)
		SHADER_PARAMETER_SRV(StructuredBuffer<FPlayerState>, rPlayerState)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FMouseSpriteVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMouseSpriteVS);
	SHADER_USE_PARAMETER_STRUCT(FMouseSpriteVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector2f, MousePos)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FEnemySpriteVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FEnemySpriteVS);
	SHADER_USE_PARAMETER_STRUCT(FEnemySpriteVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FEnemyState>, rEnemys)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FBulletSpriteVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBulletSpriteVS);
	SHADER_USE_PARAMETER_STRUCT(FBulletSpriteVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FBulletState>, rBullets)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FBombSpriteVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBombSpriteVS);
	SHADER_USE_PARAMETER_STRUCT(FBombSpriteVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FBombState>, rBombs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTitleVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTitleVS);
	SHADER_USE_PARAMETER_STRUCT(FTitleVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGameState>, rGameState)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FGameOverVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGameOverVS);
	SHADER_USE_PARAMETER_STRUCT(FGameOverVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGameState>, rGameState)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FScoreLabelVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FScoreLabelVS);
	SHADER_USE_PARAMETER_STRUCT(FScoreLabelVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGameState>, rGameState)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FScoreVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FScoreVS);
	SHADER_USE_PARAMETER_STRUCT(FScoreVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGameState>, rGameState)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FTextureSpritePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTextureSpritePS);
	SHADER_USE_PARAMETER_STRUCT(FTextureSpritePS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, tSprite)
		SHADER_PARAMETER_SAMPLER(SamplerState, SpriteSampler)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FAlphaSpritePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAlphaSpritePS);
	SHADER_USE_PARAMETER_STRUCT(FAlphaSpritePS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, Color)
		SHADER_PARAMETER_TEXTURE(Texture2D, tSprite)
		SHADER_PARAMETER_SAMPLER(SamplerState, SpriteSampler)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
class FColorSpritePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FColorSpritePS);
	SHADER_USE_PARAMETER_STRUCT(FColorSpritePS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, Color)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
	}
};
IMPLEMENT_SHADER_TYPE(, FFullScreenVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("FullScreenVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FRenderStagePS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("RenderStagePS"), SF_Pixel)
IMPLEMENT_SHADER_TYPE(, FPlayerSpriteVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("PlayerSpriteVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FBarrierSpriteVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("BarrierSpriteVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FMouseSpriteVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("MouseSpriteVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FEnemySpriteVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("EnemySpriteVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FBulletSpriteVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("BulletSpriteVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FBombSpriteVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("BombSpriteVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FTitleVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("TitleVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FGameOverVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("GameOverVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FScoreLabelVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("ScoreLabelVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FScoreVS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("ScoreVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FTextureSpritePS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("TextureSpritePS"), SF_Pixel)
IMPLEMENT_SHADER_TYPE(, FAlphaSpritePS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("AlphaSpritePS"), SF_Pixel)
IMPLEMENT_SHADER_TYPE(, FColorSpritePS, TEXT("/Plugin/BsGame/Private/BsRender.usf"), TEXT("ColorSpritePS"), SF_Pixel)


static void InitGame_Impl(
	FRHICommandListImmediate& RHICmdList,
	FBsGameInstance* pGameInst)
{
	{
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FInitGameCS> ComputeShader(GlobalShaderMap);

		{
			FRHIResourceCreateInfo CreateInfo(TEXT("GameState"));
			pGameInst->GameStateBuffer = RHICmdList.CreateStructuredBuffer(sizeof(FGameState), sizeof(FGameState), BUF_Static | BUF_UnorderedAccess, CreateInfo);
			pGameInst->GameStateSRV = RHICmdList.CreateShaderResourceView(pGameInst->GameStateBuffer);
			pGameInst->GameStateUAV = RHICmdList.CreateUnorderedAccessView(pGameInst->GameStateBuffer, false, false);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("PlayerState"));
			pGameInst->PlayerStateBuffer = RHICmdList.CreateStructuredBuffer(sizeof(FPlayerState), sizeof(FPlayerState), BUF_Static | BUF_UnorderedAccess, CreateInfo);
			pGameInst->PlayerStateSRV = RHICmdList.CreateShaderResourceView(pGameInst->PlayerStateBuffer);
			pGameInst->PlayerStateUAV = RHICmdList.CreateUnorderedAccessView(pGameInst->PlayerStateBuffer, false, false);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("Stage"));
			uint32 size = sizeof(int) * BS_SCREEN_WIDTH * BS_INIT_LAND_HEIGHT;
			pGameInst->StageBuffer = RHICmdList.CreateStructuredBuffer(sizeof(uint32), size, BUF_Static | BUF_UnorderedAccess, CreateInfo);
			pGameInst->StageSRV = RHICmdList.CreateShaderResourceView(pGameInst->StageBuffer);
			pGameInst->StageUAV = RHICmdList.CreateUnorderedAccessView(pGameInst->StageBuffer, false, false);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("RandomBuffer"));
			pGameInst->RandomBuffer = RHICmdList.CreateStructuredBuffer(sizeof(float), sizeof(float) * 65536, BUF_Static | BUF_ShaderResource, CreateInfo);
			pGameInst->RandomSRV = RHICmdList.CreateShaderResourceView(pGameInst->RandomBuffer);

			float* MappedBuffer = (float*)RHICmdList.LockBuffer(pGameInst->RandomBuffer, 0, sizeof(float) * 65536, RLM_WriteOnly);
			FDateTime t = FDateTime::Now();
			FMath::SRandInit((int32)t.GetTicks());
			for (int i = 0; i < 65536; i++, MappedBuffer++)
			{
				*MappedBuffer = FMath::SRand();
			}
			RHICmdList.UnlockBuffer(pGameInst->RandomBuffer);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("RandomCount"));
			pGameInst->RandomCountBuffer = RHICmdList.CreateStructuredBuffer(sizeof(uint32), sizeof(uint32), BUF_Static | BUF_UnorderedAccess, CreateInfo);
			pGameInst->RandomCountUAV = RHICmdList.CreateUnorderedAccessView(pGameInst->RandomCountBuffer, false, false);
		}
		pGameInst->FlipIndex = 0;
		for (int i = 0; i < 2; i++)
		{
			{
				FRHIResourceCreateInfo CreateInfo(TEXT("Enemy"));
				uint32 size = sizeof(FEnemyState) * BS_ENEMY_MAX;
				pGameInst->EnemyBuffers[i] = RHICmdList.CreateStructuredBuffer(sizeof(FEnemyState), size, BUF_Static | BUF_UnorderedAccess, CreateInfo);
				pGameInst->EnemySRVs[i] = RHICmdList.CreateShaderResourceView(pGameInst->EnemyBuffers[i]);
				pGameInst->EnemyUAVs[i] = RHICmdList.CreateUnorderedAccessView(pGameInst->EnemyBuffers[i], false, false);
			}
			{
				FRHIResourceCreateInfo CreateInfo(TEXT("Bullet"));
				uint32 size = sizeof(FBulletState) * BS_ENEMY_MAX;
				pGameInst->EnemyBulletBuffers[i] = RHICmdList.CreateStructuredBuffer(sizeof(FBulletState), size, BUF_Static | BUF_UnorderedAccess, CreateInfo);
				pGameInst->EnemyBulletSRVs[i] = RHICmdList.CreateShaderResourceView(pGameInst->EnemyBulletBuffers[i]);
				pGameInst->EnemyBulletUAVs[i] = RHICmdList.CreateUnorderedAccessView(pGameInst->EnemyBulletBuffers[i], false, false);
			}
			{
				FRHIResourceCreateInfo CreateInfo(TEXT("ReBullet"));
				uint32 size = sizeof(FBulletState) * BS_ENEMY_MAX;
				pGameInst->ReBulletBuffers[i] = RHICmdList.CreateStructuredBuffer(sizeof(FBulletState), size, BUF_Static | BUF_UnorderedAccess, CreateInfo);
				pGameInst->ReBulletSRVs[i] = RHICmdList.CreateShaderResourceView(pGameInst->ReBulletBuffers[i]);
				pGameInst->ReBulletUAVs[i] = RHICmdList.CreateUnorderedAccessView(pGameInst->ReBulletBuffers[i], false, false);
			}
			{
				FRHIResourceCreateInfo CreateInfo(TEXT("Bomb"));
				uint32 size = sizeof(FBombState) * BS_ENEMY_MAX;
				pGameInst->BombBuffers[i] = RHICmdList.CreateStructuredBuffer(sizeof(FBombState), size, BUF_Static | BUF_UnorderedAccess, CreateInfo);
				pGameInst->BombSRVs[i] = RHICmdList.CreateShaderResourceView(pGameInst->BombBuffers[i]);
				pGameInst->BombUAVs[i] = RHICmdList.CreateUnorderedAccessView(pGameInst->BombBuffers[i], false, false);
			}
			{
				FRHIResourceCreateInfo CreateInfo(TEXT("Sound"));
				uint32 size = sizeof(uint32) * BS_SOUND_MAX;
				pGameInst->SoundBuffers[i] = RHICmdList.CreateStructuredBuffer(sizeof(uint32), size, BUF_Static | BUF_UnorderedAccess, CreateInfo);
				pGameInst->SoundUAVs[i] = RHICmdList.CreateUnorderedAccessView(pGameInst->SoundBuffers[i], false, false);
			}
			{
				FRHIResourceCreateInfo CreateInfo(TEXT("Count"));
				uint32 size = sizeof(uint32) * 5;
				pGameInst->CountBuffers[i] = RHICmdList.CreateStructuredBuffer(sizeof(uint32), size, BUF_Static | BUF_UnorderedAccess, CreateInfo);
				pGameInst->CountSRVs[i] = RHICmdList.CreateShaderResourceView(pGameInst->CountBuffers[i]);
				pGameInst->CountUAVs[i] = RHICmdList.CreateUnorderedAccessView(pGameInst->CountBuffers[i], false, false);
			}
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("Argument"));
			uint32 size = sizeof(uint32) * ((3 + 4) * 4);
			pGameInst->ArgumentBuffer = RHICmdList.CreateStructuredBuffer(sizeof(uint32), size, BUF_Static | BUF_UnorderedAccess, CreateInfo);
			pGameInst->ArgumentUAV = RHICmdList.CreateUnorderedAccessView(pGameInst->ArgumentBuffer, false, false);
		}
		{
			pGameInst->SoundReadbacks.AddZeroed(4);
			for (int i = 0; i < 4; i++)
			{
				pGameInst->SoundReadbacks[i] = new FRHIGPUBufferReadback(TEXT("SoundReadback"));
			}
			pGameInst->SoundUnsed.Add(0);
			pGameInst->SoundUnsed.Add(1);
			pGameInst->SoundUnsed.Add(2);
			pGameInst->SoundUnsed.Add(3);
		}

		FInitGameCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rwPlayerState = pGameInst->PlayerStateUAV;
		Params.rwStage = pGameInst->StageUAV;
		Params.rwCount0 = pGameInst->CountUAVs[0];
		Params.rwCount1 = pGameInst->CountUAVs[1];
		Params.rwRandomCount = pGameInst->RandomCountUAV;

		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Params, FIntVector(1, 1, 1));
	}
}

static void TickGame_Impl(
	FRHICommandListImmediate& RHICmdList,
	FBsGameInstance* pGameInst,
	int LeftRightMove,
	FVector2D MousePos,
	bool bPressAttack)
{
	int PrevIndex = pGameInst->FlipIndex;
	pGameInst->FlipIndex = 1 - pGameInst->FlipIndex;
	int CurrIndex = pGameInst->FlipIndex;
	
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	// transition.
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->GameStateBuffer, ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->PlayerStateBuffer, ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->StageBuffer, ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->ArgumentBuffer, ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->CountBuffers[PrevIndex], ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->CountBuffers[CurrIndex], ERHIAccess::Unknown, ERHIAccess::UAVMask));

	// change game state
	{
		TShaderMapRef<FStartGameCS> ComputeShader(GlobalShaderMap);

		FStartGameCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rwPlayerState = pGameInst->PlayerStateUAV;
		Params.rwStage = pGameInst->StageUAV;
		Params.rwCount0 = pGameInst->CountUAVs[0];
		Params.rwCount1 = pGameInst->CountUAVs[1];
		Params.rwSound = pGameInst->SoundUAVs[CurrIndex];

		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Params, FIntVector(1, 1, 1));

	}
	
	// tick start.
	{
		TShaderMapRef<FTickStartCS> ComputeShader(GlobalShaderMap);

		FTickStartCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rCount = pGameInst->CountSRVs[PrevIndex];
		Params.rwCount = pGameInst->CountUAVs[CurrIndex];
		Params.rwArgument = pGameInst->ArgumentUAV;

		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Params, FIntVector(1, 1, 1));
	}
	
	// tick player.
	{
		TShaderMapRef<FTickPlayerCS> ComputeShader(GlobalShaderMap);

		FTickPlayerCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rwPlayerState = pGameInst->PlayerStateUAV;
		Params.rwStage = pGameInst->StageUAV;
		Params.rwSound = pGameInst->SoundUAVs[CurrIndex];
		Params.LeftRightMove = (float)LeftRightMove;
		Params.MousePos.X = (float)MousePos.X;
		Params.MousePos.Y = (float)MousePos.Y;
		Params.bPressAttack = bPressAttack ? 1 : 0;

		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Params, FIntVector(1, 1, 1));
	}
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->ArgumentBuffer, ERHIAccess::UAVMask, ERHIAccess::SRVMask));

	// tick re bullet.
	{
		TShaderMapRef<FTickReBulletCS> ComputeShader(GlobalShaderMap);

		FTickReBulletCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rwEnemys = pGameInst->EnemyUAVs[PrevIndex];
		Params.rReBullets = pGameInst->ReBulletSRVs[PrevIndex];
		Params.rwReBullets = pGameInst->ReBulletUAVs[CurrIndex];
		Params.rwStage = pGameInst->StageUAV;
		Params.rCount = pGameInst->CountSRVs[PrevIndex];
		Params.rwCount = pGameInst->CountUAVs[CurrIndex];
		Params.rRandom = pGameInst->RandomSRV;
		Params.rwRandomCount = pGameInst->RandomCountUAV;

		FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, Params, pGameInst->ArgumentBuffer, BS_ADDRESS_RE_BULLET_CS_ARG * sizeof(int32));
	}

	// tick enemy bullet.
	{
		TShaderMapRef<FTickEnemyBulletCS> ComputeShader(GlobalShaderMap);

		FTickEnemyBulletCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rwPlayerState = pGameInst->PlayerStateUAV;
		Params.rBullets = pGameInst->EnemyBulletSRVs[PrevIndex];
		Params.rwBullets = pGameInst->EnemyBulletUAVs[CurrIndex];
		Params.rwReBullets = pGameInst->ReBulletUAVs[CurrIndex];
		Params.rwBombs = pGameInst->BombUAVs[CurrIndex];
		Params.rwStage = pGameInst->StageUAV;
		Params.rCount = pGameInst->CountSRVs[PrevIndex];
		Params.rwCount = pGameInst->CountUAVs[CurrIndex];
		Params.rRandom = pGameInst->RandomSRV;
		Params.rwRandomCount = pGameInst->RandomCountUAV;
		Params.rwSound = pGameInst->SoundUAVs[CurrIndex];

		FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, Params, pGameInst->ArgumentBuffer, BS_ADDRESS_ENEMY_BULLET_CS_ARG * sizeof(int32));
	}
	
	// tick enemy.
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->EnemyBuffers[PrevIndex], ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->EnemyBuffers[CurrIndex], ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->BombBuffers[CurrIndex], ERHIAccess::Unknown, ERHIAccess::UAVMask));
	{
		TShaderMapRef<FTickEnemyCS> ComputeShader(GlobalShaderMap);

		FTickEnemyCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rEnemys = pGameInst->EnemySRVs[PrevIndex];
		Params.rwEnemys = pGameInst->EnemyUAVs[CurrIndex];
		Params.rwBullets = pGameInst->EnemyBulletUAVs[CurrIndex];
		Params.rwBombs = pGameInst->BombUAVs[CurrIndex];
		Params.rCount = pGameInst->CountSRVs[PrevIndex];
		Params.rwCount = pGameInst->CountUAVs[CurrIndex];
		Params.rRandom = pGameInst->RandomSRV;
		Params.rwRandomCount = pGameInst->RandomCountUAV;
		Params.rwSound = pGameInst->SoundUAVs[CurrIndex];

		FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, Params, pGameInst->ArgumentBuffer, BS_ADDRESS_ENEMY_CS_ARG * sizeof(int32));
	}
	{
		TShaderMapRef<FTickAddEnemyCS> ComputeShader(GlobalShaderMap);

		FTickAddEnemyCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rwEnemys = pGameInst->EnemyUAVs[CurrIndex];
		Params.rwCount = pGameInst->CountUAVs[CurrIndex];
		Params.rRandom = pGameInst->RandomSRV;
		Params.rwRandomCount = pGameInst->RandomCountUAV;

		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Params, FIntVector(1, 1, 1));
	}
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->EnemyBuffers[CurrIndex], ERHIAccess::UAVMask, ERHIAccess::SRVMask));

	// tick bomb.
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->BombBuffers[PrevIndex], ERHIAccess::Unknown, ERHIAccess::SRVMask));
	{
		TShaderMapRef<FTickBombCS> ComputeShader(GlobalShaderMap);

		FTickBombCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rBombs = pGameInst->BombSRVs[PrevIndex];
		Params.rwBombs = pGameInst->BombUAVs[CurrIndex];
		Params.rCount = pGameInst->CountSRVs[PrevIndex];
		Params.rwCount = pGameInst->CountUAVs[CurrIndex];

		FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, Params, pGameInst->ArgumentBuffer, BS_ADDRESS_BOMB_CS_ARG * sizeof(int32));
	}
	
	// tick end.
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->ArgumentBuffer, ERHIAccess::SRVMask, ERHIAccess::UAVMask));
	{
		TShaderMapRef<FTickEndCS> ComputeShader(GlobalShaderMap);

		FTickEndCS::FParameters Params;
		Params.rwGameState = pGameInst->GameStateUAV;
		Params.rwCount = pGameInst->CountUAVs[CurrIndex];
		Params.rwArgument = pGameInst->ArgumentUAV;

		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Params, FIntVector(1, 1, 1));
	}
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->CountBuffers[CurrIndex], ERHIAccess::UAVMask, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->ArgumentBuffer, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
	
	// transition.
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->GameStateBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->PlayerStateBuffer, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->StageBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(pGameInst->BombBuffers[CurrIndex], ERHIAccess::UAVMask, ERHIAccess::SRVMask));

	// readback sound request.
	if (pGameInst->SoundUnsed.Num() > 0)
	{
		int Index = pGameInst->SoundUnsed[0];
		pGameInst->SoundUnsed.RemoveAt(0);
		pGameInst->SoundPending.Add(Index);
		auto Readback = pGameInst->SoundReadbacks[Index];
		Readback->EnqueueCopy(RHICmdList, pGameInst->SoundBuffers[CurrIndex], BS_SOUND_MAX * sizeof(uint32));

		UE_LOG(LogTemp, Warning, TEXT("Pending <- %d"), Index);
	}
	bool bSoundReset = true;
	if (pGameInst->SoundPending.Num() > 0)
	{
		int Index = pGameInst->SoundPending[0];
		auto Readback = pGameInst->SoundReadbacks[Index];
		if (Readback->IsReady())
		{
			pGameInst->SoundPending.RemoveAt(0);
			pGameInst->SoundUnsed.Add(Index);
			uint32* p = (uint32*)Readback->Lock(BS_SOUND_MAX * sizeof(uint32));
			pGameInst->TitleBGM = p[BS_SOUND_TITLE];
			pGameInst->GameOverBGM = p[BS_SOUND_GAMEOVER];
			pGameInst->SelectSE = p[BS_SOUND_SELECT];
			pGameInst->ShotSE = p[BS_SOUND_SHOT];
			pGameInst->BombSE = p[BS_SOUND_BOMB];
			pGameInst->ReflectSE = p[BS_SOUND_REFLECT];
			pGameInst->PushSE = p[BS_SOUND_PUSH];
			Readback->Unlock();

			bSoundReset = false;
			UE_LOG(LogTemp, Warning, TEXT("Unused <- %d"), Index);
		}
	}
	if (bSoundReset)
	{
		// sound reset.
		pGameInst->TitleBGM = pGameInst->GameOverBGM = 0;
		pGameInst->SelectSE = pGameInst->ShotSE = pGameInst->BombSE = pGameInst->ReflectSE = pGameInst->PushSE = 0;
	}
}

static void RenderGame_Impl(
	FRHICommandListImmediate& RHICmdList,
	FBsGameInstance* pGameInst,
	const FBsGameAssets* pAssets,
	FVector2D MousePos,
	FTextureRenderTargetResource* TargetRes)
{
	FRHITexture2D* RTTexture = TargetRes->GetRenderTargetTexture();
	RHICmdList.Transition(FRHITransitionInfo(RTTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

	FRHITexture* RT = RTTexture;
	uint32 w = TargetRes->GetSizeX();
	uint32 h = TargetRes->GetSizeY();

	int CurrIndex = pGameInst->FlipIndex;
	
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	// render stage.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderStage"));
		{
			FIntPoint TargetResolution(w, h);

			// Update viewport.
			RHICmdList.SetViewport(
				0, 0, 0.f,
				TargetResolution.X, TargetResolution.Y, 1.f);

			// Get shaders.
			TShaderMapRef<FFullScreenVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FRenderStagePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			FRenderStagePS::FParameters Params;
			Params.rStage = pGameInst->StageSRV;
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);

			// Draw grid.
			RHICmdList.DrawPrimitive(0, 3, 1);
		}
		RHICmdList.EndRenderPass();
	}

	// render player.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderPlayer"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FPlayerSpriteVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FTextureSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FPlayerSpriteVS::FParameters Params;
				Params.rGameState = pGameInst->GameStateSRV;
				Params.rPlayerState = pGameInst->PlayerStateSRV;
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FTextureSpritePS::FParameters Params;
				Params.tSprite = pAssets->PlayerSprite->GetResource()->GetTexture2DRHI();
				Params.SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitive(0, 4, 1);
		}
		RHICmdList.EndRenderPass();
	}

	// render barrier.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderBarrier"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FBarrierSpriteVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FColorSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FBarrierSpriteVS::FParameters Params;
				Params.rGameState = pGameInst->GameStateSRV;
				Params.rPlayerState = pGameInst->PlayerStateSRV;
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FColorSpritePS::FParameters Params;
				Params.Color = FVector4f(1.0f, 1.0f, 0.0f, 0.8f);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitive(0, 4, 1);
		}
		RHICmdList.EndRenderPass();
	}

	// render re bullets.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderReBullets"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FBulletSpriteVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FColorSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FBulletSpriteVS::FParameters Params;
				Params.rBullets = pGameInst->ReBulletSRVs[CurrIndex];
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FColorSpritePS::FParameters Params;
				Params.Color = FVector4f(0.0f, 0.6f, 0.9f, 1.0f);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitiveIndirect(pGameInst->ArgumentBuffer, BS_ADDRESS_RE_BULLET_VS_ARG * sizeof(int32));
		}
		RHICmdList.EndRenderPass();
	}

	// render enemy bullets.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderEnemyBullets"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FBulletSpriteVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FColorSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FBulletSpriteVS::FParameters Params;
				Params.rBullets = pGameInst->EnemyBulletSRVs[CurrIndex];
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FColorSpritePS::FParameters Params;
				Params.Color = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitiveIndirect(pGameInst->ArgumentBuffer, BS_ADDRESS_ENEMY_BULLET_VS_ARG * sizeof(int32));
		}
		RHICmdList.EndRenderPass();
	}

	// render enemys.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderEnemys"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FEnemySpriteVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FTextureSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FEnemySpriteVS::FParameters Params;
				Params.rEnemys = pGameInst->EnemySRVs[CurrIndex];
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FTextureSpritePS::FParameters Params;
				Params.tSprite = pAssets->EnemySprite->GetResource()->GetTexture2DRHI();
				Params.SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitiveIndirect(pGameInst->ArgumentBuffer, BS_ADDRESS_ENEMY_VS_ARG * sizeof(int32));
		}
		RHICmdList.EndRenderPass();
	}

	// render bombs.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderBombs"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FBombSpriteVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FTextureSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FBombSpriteVS::FParameters Params;
				Params.rBombs = pGameInst->BombSRVs[CurrIndex];
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FTextureSpritePS::FParameters Params;
				Params.tSprite = pAssets->BombSprite->GetResource()->GetTexture2DRHI();
				Params.SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitiveIndirect(pGameInst->ArgumentBuffer, BS_ADDRESS_BOMB_VS_ARG * sizeof(int32));
		}
		RHICmdList.EndRenderPass();
	}

	// render mouse pos.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderMousePos"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FMouseSpriteVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FColorSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FMouseSpriteVS::FParameters Params;
				Params.MousePos.X = (float)MousePos.X;
				Params.MousePos.Y = (float)MousePos.Y;
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FColorSpritePS::FParameters Params;
				Params.Color = FVector4f(1.0f, 0.0f, 0.0f, 1.0f);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitive(0, 4, 1);
		}
		RHICmdList.EndRenderPass();
	}

	// render title.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderTitle"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FTitleVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FAlphaSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FTitleVS::FParameters Params;
				Params.rGameState = pGameInst->GameStateSRV;
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FAlphaSpritePS::FParameters Params;
				Params.Color = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
				Params.tSprite = pAssets->TitleSprite->GetResource()->GetTexture2DRHI();
				Params.SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitive(0, 4, 2);
		}
		RHICmdList.EndRenderPass();
	}

	// render gameover.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderGameOver"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FGameOverVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FAlphaSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FGameOverVS::FParameters Params;
				Params.rGameState = pGameInst->GameStateSRV;
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FAlphaSpritePS::FParameters Params;
				Params.Color = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
				Params.tSprite = pAssets->GameOverSprite->GetResource()->GetTexture2DRHI();
				Params.SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitive(0, 4, 1);
		}
		RHICmdList.EndRenderPass();
	}

	// render score label.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderScoreLabel"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FScoreLabelVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FAlphaSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FScoreLabelVS::FParameters Params;
				Params.rGameState = pGameInst->GameStateSRV;
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FAlphaSpritePS::FParameters Params;
				Params.Color = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
				Params.tSprite = pAssets->FontSprite->GetResource()->GetTexture2DRHI();
				Params.SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitive(0, 4, 2);
		}
		RHICmdList.EndRenderPass();
	}

	// render score.
	{
		FRHIRenderPassInfo RPInfo(1, &RT, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RenderScore"));
		{
			FIntPoint TargetResolution(w, h);

			// Get shaders.
			TShaderMapRef<FScoreVS> VertexShader(GlobalShaderMap);
			TShaderMapRef<FAlphaSpritePS> PixelShader(GlobalShaderMap);

			// Set the graphic pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			{
				FScoreVS::FParameters Params;
				Params.rGameState = pGameInst->GameStateSRV;
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Params);
			}
			{
				FAlphaSpritePS::FParameters Params;
				Params.Color = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
				Params.tSprite = pAssets->FontSprite->GetResource()->GetTexture2DRHI();
				Params.SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Params);
			}

			// Draw grid.
			RHICmdList.DrawPrimitive(0, 4, 14);
		}
		RHICmdList.EndRenderPass();
	}

	RHICmdList.Transition(FRHITransitionInfo(RTTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
}

void UBsGameFunctionLibrary::InitGame(
	FBsGameInstance& GameInst)
{
	FBsGameInstance* pGameInst = &GameInst;
	ENQUEUE_RENDER_COMMAND(InitGame)(
		[pGameInst](FRHICommandListImmediate& RHICmdList)
		{
			InitGame_Impl(RHICmdList, pGameInst);
		}
	);
}

void UBsGameFunctionLibrary::TickGame(
	FBsGameInstance& GameInst,
	int LeftRightMove,
	FVector2D MousePos,
	bool bPressAttack)
{
	FBsGameInstance* pGameInst = &GameInst;
	ENQUEUE_RENDER_COMMAND(TickGame)(
		[pGameInst, LeftRightMove, MousePos, bPressAttack](FRHICommandListImmediate& RHICmdList)
		{
			TickGame_Impl(RHICmdList, pGameInst, LeftRightMove, MousePos, bPressAttack);
		}
	);
}
	
void UBsGameFunctionLibrary::RenderGame(
	FBsGameInstance& GameInst,
	const FBsGameAssets& Assets,
	FVector2D MousePos,
	class UTextureRenderTarget2D* TargetRT)
{
	FBsGameInstance* pGameInst = &GameInst;
	const FBsGameAssets* pAssets = &Assets;
	FTextureRenderTargetResource* TargetRes = TargetRT->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(RenderGame)(
		[pGameInst, pAssets, MousePos, TargetRes](FRHICommandListImmediate& RHICmdList)
		{
			RenderGame_Impl(RHICmdList, pGameInst, pAssets, MousePos, TargetRes);
		}
	);
}

void UBsGameFunctionLibrary::EndGame(
	FBsGameInstance& GameInst)
{
	for (auto buffer : GameInst.SoundReadbacks)
	{
		if (buffer)
		{
			delete buffer;
		}
	}
	GameInst.SoundReadbacks.Empty();
}
