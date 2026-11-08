// Quirrel (.nut) Runtime Script executed natively by Gaijin Quirrel VM (prog/1stPartyLibs/quirrel)
let dagor_rt = {
  engine = "Gaijin Dagor Engine 6.5 (daRg + Quirrel VM + daSkies2)"
  hangarTitle = "ПОЛИГОН НИКИНЦИ • ОТКРЫТАЯ БАЗА DAGOR 6.5"
  battleBtn = "В БОЙ!"
  skiesPresets = [
    "ЗАКАТ daSkies (Golden Hour)"
    "ПОЛДЕНЬ daSkies (Clear Sky)"
    "ГРОЗА daSkies (Storm Front)"
    "НОЧЬ daSkies (daStars / ПНВ)"
  ]
  los_armor = @(nominalMm, angleDeg) (nominalMm / ::cos(angleDeg * 3.14159265 / 180.0)).tointeger()
  status_banner = @() "Gaijin Dagor Engine 6.5 (daRg + Quirrel VM + daSkies2)"
}

::getroottable()["dagor_rt"] <- dagor_rt
return dagor_rt.status_banner()
