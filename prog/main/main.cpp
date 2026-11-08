// Точка входа клиента/сервера Steel Front: Online на базе Gaijin daNetGame (Dagor Engine)
#include <daECS/core/entityManager.h>
#include <daECS/core/componentTypes.h>
#include <startup/dag_globalSettings.h>
#include <ioSys/dag_dataBlock.h>
#include <debug/dag_debug.h>

namespace steel_front
{
void register_tank_game_modules()
{
  const DataBlock *cfg = ::dgs_get_settings();
  const char *proj = cfg ? cfg->getStr("projectName", "steel_front_online") : "steel_front_online";
  debug("[SteelFront/Dagor] Инициализация танковых ECS-систем и сетевой репликации для %s", proj);
}
} // namespace steel_front

// Хук инициализации игрового модуля daNetGame
void game_init_custom_systems()
{
  steel_front::register_tank_game_modules();
}
