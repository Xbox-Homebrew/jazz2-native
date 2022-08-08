﻿#include "AmmoCrate.h"
#include "../../LevelInitialization.h"
#include "../../ILevelHandler.h"
#include "../../Tiles/TileMap.h"
#include "../Player.h"
#include "../Weapons/ShotBase.h"

#include "../../../nCine/Base/Random.h"

namespace Jazz2::Actors::Solid
{
	AmmoCrate::AmmoCrate()
	{
	}

	void AmmoCrate::Preload(const ActorActivationDetails& details)
	{
		PreloadMetadataAsync("Object/CrateContainer"_s);
	}

	Task<bool> AmmoCrate::OnActivatedAsync(const ActorActivationDetails& details)
	{
		Movable = true;
		CollisionFlags |= CollisionFlags::SkipPerPixelCollisions;

		WeaponType weaponType = (WeaponType)details.Params[0];
		if (weaponType != WeaponType::Blaster) {
			AddContent(EventType::Ammo, 5, &details.Params[0], 1);
		}

		co_await RequestMetadataAsync("Object/CrateContainer"_s);

		switch (weaponType) {
			case WeaponType::Bouncer: SetAnimation("CrateAmmoBouncer"_s); break;
			case WeaponType::Freezer: SetAnimation("CrateAmmoFreezer"_s); break;
			case WeaponType::Seeker:SetAnimation("CrateAmmoSeeker"_s); break;
			case WeaponType::RF: SetAnimation("CrateAmmoRF"_s); break;
			case WeaponType::Toaster: SetAnimation("CrateAmmoToaster"_s); break;
			case WeaponType::TNT:SetAnimation("CrateAmmoTNT"_s); break;
			case WeaponType::Pepper: SetAnimation("CrateAmmoPepper"_s); break;
			case WeaponType::Electro: SetAnimation("CrateAmmoElectro"_s); break;
			case WeaponType::Thunderbolt: SetAnimation("CrateAmmoThunderbolt"_s); break;
			default: SetAnimation(AnimState::Idle); break;
		}

		co_return true;
	}

	bool AmmoCrate::OnHandleCollision(ActorBase* other)
	{
		if (_health == 0) {
			return GenericContainer::OnHandleCollision(other);
		}

		if (auto shotBase = dynamic_cast<Weapons::ShotBase*>(other)) {
			DecreaseHealth(shotBase->GetStrength(), other);
			shotBase->DecreaseHealth(INT32_MAX);
			return true;
		} /*else if (auto shotTnt = dynamic_cast<Weapons::ShotTNT*>(other)) {
			// TODO: TNT
		}*/ else if (auto player = dynamic_cast<Player*>(other)) {
			if (player->CanBreakSolidObjects()) {
				DecreaseHealth(INT32_MAX, other);
				return true;
			}
		}

		return GenericContainer::OnHandleCollision(other);
	}

	bool AmmoCrate::OnPerish(ActorBase* collider)
	{
		CollisionFlags = CollisionFlags::None;

		CreateParticleDebris();

		PlaySfx("Break"_s);

		if (_content.empty()) {
			// Random Ammo create
			SmallVector<WeaponType, (int)WeaponType::Count> weaponTypes;
			auto& players = _levelHandler->GetPlayers();
			for (auto& player : players) {
				const auto playerAmmo = player->GetWeaponAmmo();
				for (int i = 1; i < PlayerCarryOver::WeaponCount; i++) {
					if (playerAmmo[i] > 0) {
						weaponTypes.push_back((WeaponType)i);
					}
				}
			}

			if (weaponTypes.empty()) {
				weaponTypes.push_back(WeaponType::Bouncer);
			}

			int n = Random().Next(4, 7);
			for (int i = 0; i < n; i++) {
				uint8_t weaponType = (uint8_t)weaponTypes[Random().Next(0, weaponTypes.size())];
				AddContent(EventType::Ammo, 1, &weaponType, sizeof(weaponType));
			}

			CreateSpriteDebris("CrateShrapnel1"_s, 3);
			CreateSpriteDebris("CrateShrapnel2"_s, 2);

			SetTransition(AnimState::TransitionDeath, false, [this, collider]() {
				GenericContainer::OnPerish(collider);
			});
			SpawnContent();
			return true;
		} else {
			CreateSpriteDebris("CrateAmmoShrapnel1", 3);
			CreateSpriteDebris("CrateAmmoShrapnel2", 2);

			return GenericContainer::OnPerish(collider);
		}
	}
}