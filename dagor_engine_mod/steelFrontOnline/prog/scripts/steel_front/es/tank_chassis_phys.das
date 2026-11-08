require ecs
require math
require DagorMath

// Гусеничная физика и двухкруговое наведение башни за курсором мыши (Mouse Aim)
[es(no_order)]
def tank_turret_mouse_aim_es(info : UpdateStageInfoAct;
                             transform : float3x4;
                             tank__aimWorldPos : float3;
                             tank__turretTurnDeg : float;
                             tank__modBreech : float;
                             var tank__turretYaw : float&;
                             var tank__gunPitch : float&;
                             var tank__reloadTimer : float&)
  if tank__reloadTimer > 0.0
    tank__reloadTimer = max(0.0, tank__reloadTimer - info.dt)

  let turretOrigin = transform[3] + float3(0.0, 1.72, 0.0)
  let delta = tank__aimWorldPos - turretOrigin
  let desiredYaw = atan2(-delta.x, -delta.z)
  let maxStep = deg_to_rad(tank__turretTurnDeg) * info.dt
  var diff = desiredYaw - tank__turretYaw
  while diff > PI
    diff -= TWOPI
  while diff < -PI
    diff += TWOPI

  if abs(diff) <= maxStep
    tank__turretYaw = desiredYaw
  else
    tank__turretYaw += sign(diff) * maxStep

  let horiz = max(1.0, length(float2(delta.x, delta.z)))
  let targetPitch = clamp(atan2(delta.y, horiz), -0.14, 0.36)
  tank__gunPitch = lerp(tank__gunPitch, targetPitch, min(1.0, info.dt * 8.0))
  let _ = tank__modBreech
