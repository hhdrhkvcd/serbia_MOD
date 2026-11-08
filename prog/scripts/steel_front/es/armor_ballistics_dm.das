require ecs
require math
require DagorMath

// Серверная система баллистики, приведённой брони, рикошетов и повреждения модулей (War Thunder DM)
def calc_effective_armor_mm(nominal_mm : float; cos_impact : float; shell_type : string; era_type : string) : float
  let clampedCos = max(0.34, abs(cos_impact))
  let slopeArmor = nominal_mm / clampedCos
  var eraBonus = 0.0
  if era_type != ""
    if shell_type == "heatfs"
      eraBonus = 190.0
    elif shell_type == "apfsds"
      eraBonus = 40.0
    elif shell_type == "atgm"
      eraBonus = 45.0
  return slopeArmor + eraBonus

[es(tag=server, no_order)]
def step_tank_projectiles_es(info : UpdateStageInfoAct;
                             eid : EntityId;
                             proj__shooterEid : EntityId;
                             proj__shellType : string;
                             proj__penMM : float;
                             proj__damage : float;
                             proj__guided : bool;
                             var proj__pos : float3&;
                             var proj__vel : float3&;
                             var proj__ttl : float&)
  proj__ttl -= info.dt
  if proj__ttl <= 0.0
    destroyEntity(eid)
    return

  if !proj__guided
    proj__vel.y -= 9.81 * info.dt

  let nextPos = proj__pos + proj__vel * info.dt
  var hitDetected = false

  query() <| $ [es] (eid aka target_eid : EntityId;
                     transform : float3x4;
                     tank__hullArmorMM : float3;
                     tank__turretArmorMM : float3;
                     tank__eraType : string;
                     var tank__hp : float&;
                     var tank__crewCount : int&;
                     var tank__onFire : bool&;
                     var tank__modAmmoRack : float&)
    if hitDetected || target_eid == proj__shooterEid || tank__hp <= 0.0
      return
    let center = transform[3] + float3(0.0, 1.15, 0.0)
    if distance(nextPos, center) <= 2.45
      hitDetected = true
      let isTurret = nextPos.y > transform[3].y + 1.25
      let nominal = isTurret ? tank__turretArmorMM.x : tank__hullArmorMM.x
      let dirNorm = normalize(proj__vel)
      let frontNormal = -normalize(transform[2])
      let cosImpact = dot(dirNorm, frontNormal)
      let effArmor = calc_effective_armor_mm(nominal, cosImpact, proj__shellType, tank__eraType)

      if proj__penMM >= effArmor
        tank__hp = max(0.0, tank__hp - proj__damage)
        if nextPos.y < transform[3].y + 0.9
          tank__modAmmoRack = 0.0
          tank__hp = 0.0 // Детонация боекомплекта и отрыв башни
        elif tank__crewCount > 1
          tank__crewCount -= 1
          if tank__crewCount <= 1
            tank__hp = 0.0
      let _ = tank__onFire

  if hitDetected || nextPos.y < 0.0
    destroyEntity(eid)
    return

  proj__pos = nextPos
