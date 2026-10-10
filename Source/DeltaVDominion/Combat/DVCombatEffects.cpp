#include "Combat/DVCombatEffects.h"
#include "Combat/DVAttributeSet.h"
#include "DVGameplayTags.h"

namespace
{
	struct FDVDamageStatics
	{
		FGameplayEffectAttributeCaptureDefinition ArmorDef;

		FDVDamageStatics()
		{
			ArmorDef = FGameplayEffectAttributeCaptureDefinition(UDVAttributeSet::GetArmorAttribute(),
				EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot*/ false);
		}
	};

	const FDVDamageStatics& DamageStatics()
	{
		static FDVDamageStatics Statics;
		return Statics;
	}
}

UDVDamageExecution::UDVDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
}

void UDVDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float Armor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvalParams, Armor);
	Armor = FMath::Clamp(Armor, 0.f, 80.f);

	float Damage = Spec.GetSetByCallerMagnitude(DVTags::SetByCaller_Damage, /*WarnIfNotFound*/ false, 0.f);

	if (Spec.GetDynamicAssetTags().HasTag(DVTags::Damage_Headshot))
	{
		Damage *= 2.5f;
	}

	Damage *= (1.f - Armor / 100.f);
	if (Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UDVAttributeSet::GetIncomingDamageAttribute(),
			EGameplayModOp::Additive, Damage));
	}
}

UDVDamageEffect::UDVDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Exec;
	Exec.CalculationClass = UDVDamageExecution::StaticClass();
	Executions.Add(Exec);
}

UDVSuppressionEffect::UDVSuppressionEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = DVTags::SetByCaller_Suppression;

	FGameplayModifierInfo Mod;
	Mod.Attribute = UDVAttributeSet::GetIncomingSuppressionAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Mod);
}
