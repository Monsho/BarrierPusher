#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BsGameFunctionLibrary.generated.h"

USTRUCT()
struct FTargetInfo
{
	GENERATED_BODY()
	
	FVector Location;
	float RadiusRate;
};

USTRUCT(BlueprintType)
struct FBsGameInstance
{
	GENERATED_BODY()

	FBufferRHIRef GameStateBuffer;
	FShaderResourceViewRHIRef GameStateSRV;
	FUnorderedAccessViewRHIRef GameStateUAV;

	FBufferRHIRef PlayerStateBuffer;
	FShaderResourceViewRHIRef PlayerStateSRV;
	FUnorderedAccessViewRHIRef PlayerStateUAV;

	FBufferRHIRef StageBuffer;
	FShaderResourceViewRHIRef StageSRV;
	FUnorderedAccessViewRHIRef StageUAV;

	FBufferRHIRef RandomBuffer;
	FShaderResourceViewRHIRef RandomSRV;
	FBufferRHIRef RandomCountBuffer;
	FUnorderedAccessViewRHIRef RandomCountUAV;

	int FlipIndex;
	
	FBufferRHIRef EnemyBuffers[2];
	FShaderResourceViewRHIRef EnemySRVs[2];
	FUnorderedAccessViewRHIRef EnemyUAVs[2];

	FBufferRHIRef EnemyBulletBuffers[2];
	FShaderResourceViewRHIRef EnemyBulletSRVs[2];
	FUnorderedAccessViewRHIRef EnemyBulletUAVs[2];

	FBufferRHIRef ReBulletBuffers[2];
	FShaderResourceViewRHIRef ReBulletSRVs[2];
	FUnorderedAccessViewRHIRef ReBulletUAVs[2];

	FBufferRHIRef BombBuffers[2];
	FShaderResourceViewRHIRef BombSRVs[2];
	FUnorderedAccessViewRHIRef BombUAVs[2];

	FBufferRHIRef SoundBuffers[2];
	FUnorderedAccessViewRHIRef SoundUAVs[2];

	FBufferRHIRef CountBuffers[2]; // Enemy, EnemyBullet, ReBullet, Bomb, Score
	FShaderResourceViewRHIRef CountSRVs[2];
	FUnorderedAccessViewRHIRef CountUAVs[2];

	FBufferRHIRef ArgumentBuffer; // DispatchEnemy(3), DispatchBullet(3), DispatchReBullet(3), DrawEnemy(4), DrawBullet(4), DrawReBullet(4)
	FUnorderedAccessViewRHIRef ArgumentUAV;

	TArray<class FRHIGPUBufferReadback*> SoundReadbacks;
	TArray<int> SoundPending;
	TArray<int> SoundUnsed;

	UPROPERTY(BlueprintReadOnly)
	int TitleBGM = 0;

	UPROPERTY(BlueprintReadOnly)
	int GameOverBGM = 0;

	UPROPERTY(BlueprintReadOnly)
	int SelectSE = 0;

	UPROPERTY(BlueprintReadOnly)
	int ShotSE = 0;

	UPROPERTY(BlueprintReadOnly)
	int BombSE = 0;

	UPROPERTY(BlueprintReadOnly)
	int ReflectSE = 0;

	UPROPERTY(BlueprintReadOnly)
	int PushSE = 0;
};

USTRUCT(BlueprintType)
struct FBsGameAssets
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> PlayerSprite;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> EnemySprite;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> BombSprite;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> TitleSprite;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> GameOverSprite;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> FontSprite;
};

UCLASS()
class BSGAME_API UBsGameFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable)
	static void InitGame(
		UPARAM(ref) FBsGameInstance& GameInst);

	UFUNCTION(BlueprintCallable)
	static void TickGame(
		UPARAM(ref) FBsGameInstance& GameInst,
		int LeftRightMove,
		FVector2D MousePos,
		bool bPressAttack);
	
	UFUNCTION(BlueprintCallable)
	static void RenderGame(
		UPARAM(ref) FBsGameInstance& GameInst,
		const FBsGameAssets& Assets,
		FVector2D MousePos,
		class UTextureRenderTarget2D* TargetRT);

	UFUNCTION(BlueprintCallable)
	static void EndGame(
		UPARAM(ref) FBsGameInstance& GameInst);
};
