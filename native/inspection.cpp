#include "inspection.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace recvx {
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Diese Datei band ItemModelCheck,
// ItemModelMessageWait, ItemModelActionSet, ItemModelChangeZoomIn/Out,
// MdlHideCheck, FlagErase, MdlAction00/01/02, item_name_id, FileNumberSwitch,
// file_model_mask sowie die Dateitabellen (fstbl/fsheader/wallpaper) ein, die
// tools/generate_inventory_engine.py als inventory_engine.inc aus itemview.c,
// sub1.c und fileview.c erzeugt hat. Generator und Include liegen in
// archived/FORBIDDEN_DONTUSE/ und dürfen weder gelesen noch ausgeführt
// werden.
//
// Zu leisten (jeweils als eigene native Mechanik, nicht aus einem Generator
// über die Originalquellen):
//  - der Untersuchungsautomat eines Items: Richtungsprüfung, Dietrich-Sperre,
//    Bestätigungsfrage, die drei Aktionsroutinen, beide 32-Tick-Zoomphasen,
//    die Umwandlung in das Ergebnis-Item und die zugehörigen Meldungen,
//  - die Sichtbarkeitsmasken der Teilknoten (MdlHideCheck/FlagErase) und die
//    Posen der Aktionsframes,
//  - die Namensauswahl eines Inventareintrags (unbekanntes Objekt, leere
//    Waffe, gelesenes Handbuch) aus dem gepackten Slotwert,
//  - die Datei-Zuordnung: welches Item/welcher Raum welche Datei liefert,
//    welche Items reine Raummodelle sind, und die Seitenzahl-, Meldungs- und
//    Hintergrundtabellen der 24 Dateien.
//
// Solange das offen ist, melden die betroffenen Funktionen den dokumentierten
// Neutralwert: der Untersuchungsautomat startet nie (phase bleibt auf dem
// Ruhewert 2, running/question bleiben falsch, es wird keine Meldung und
// keine Inventaränderung erzeugt), inspection_geometry liefert keine
// Knotenüberschreibungen (das Paket wird unverändert gezeichnet),
// inventory_name_id liefert 0, und die Dateizuordnung meldet "keine Datei".
// Für die beiden Dateitabellen, bei denen jeder Zahlenwert sofort als echter
// Originalindex gezeichnet bzw. geladen würde (Seitentext und Hintergrund),
// wird bewusst ein Fehler gemeldet statt ein falscher Index geliefert.
//
// Sichtbare Folge: Items lassen sich nicht mehr untersuchen — Koffer, Kästen
// und die Silberlibelle öffnen sich nicht, es gibt keine Frage, keine
// Öffnungsanimation und keine Umwandlung. Versteckte Teilknoten der
// Itemmodelle werden mitgezeichnet, weil die Sichtbarkeitsmaske fehlt.
// Itemnamen im Inventar werden nicht mehr richtig ausgewählt. Der
// Dateibildschirm hat keine Seiten und ist nicht darstellbar; aufgenommene
// Items lösen keine Dateiaufnahme aus. Kein geratener Ersatzwert.
struct ItemInspection::State {
    // S_WORK testmode: 2 ist der Ruhewert, den der Originalautomat außerhalb
    // eines laufenden Vorgangs hält.
    unsigned phase=2,source=0,item=0,index=0,action_frame=0;
    bool message=false;
    std::optional<std::array<Angle,3>> display_reset;
};
ItemInspection::ItemInspection():state_(std::make_shared<State>()){}
void ItemInspection::check(Inventory& inventory,unsigned slot,std::array<Angle,3> angles){
    // Eingangsprüfung des Hosts bleibt erhalten; der Originalautomat
    // ItemModelCheck selbst fehlt, es passiert also nichts weiter.
    if(slot<1||slot>=12||!Inventory::id(inventory.slots()[slot]))throw std::runtime_error("invalid examination slot");
    (void)angles;
    state_=std::make_shared<State>();
}
void ItemInspection::answer(Inventory& inventory,bool yes){(void)inventory;(void)yes;}
void ItemInspection::cancel(Inventory& inventory,unsigned slot){(void)inventory;(void)slot;state_=std::make_shared<State>();}
void ItemInspection::tick(Inventory& inventory){(void)inventory;}
std::optional<std::array<Angle,3>> ItemInspection::take_display_reset(){auto result=state_->display_reset;state_->display_reset.reset();return result;}
bool ItemInspection::question()const{return state_->phase==6;}
bool ItemInspection::running()const{return phase()==4||phase()==5||phase()==1;}
bool ItemInspection::message_pending()const{return state_->message;}
unsigned ItemInspection::take_message(){state_->message=false;return state_->index;}
unsigned ItemInspection::source()const{return state_->source;}
unsigned ItemInspection::action_frame()const{return state_->action_frame;}
unsigned ItemInspection::item()const{return state_->item;}
unsigned ItemInspection::model()const{return inspection_model_id(state_->item);}
unsigned ItemInspection::phase()const{return state_->phase;}
unsigned ItemInspection::completed_file()const{return phase()==7?2u:24u;}
// Die beiden Zoomphasen (1/5) treten ohne den Originalautomaten nicht auf,
// die Ansicht bleibt deshalb auf voller Größe.
float ItemInspection::zoom()const{return 1.f;}
std::array<Angle,3> ItemInspection::spin()const{return {0,0,0};}
// --- Weiterhin eigenständig portierte Teile: reine Tabellen-/Matrixarbeit ---
unsigned inspection_action(unsigned item){if(item>=156)throw std::runtime_error("invalid item action");return itemflg[dsptbl[item].hide][1];}
std::array<Angle,3> inspection_default_angles(unsigned item){
    if(item>=156)throw std::runtime_error("invalid display item");
    // DrawSubItem subcsr 0/1: transpose the negative DSP_WORK X/Y/Z
    // rotations. These inspection angles describe rotmat, not a room camera.
    const auto& d=dsptbl[item];NJS_MATRIX m;njUnitMatrix(&m);
    njRotateX(&m,int(-d.ax*182.04445f)&65535);njRotateY(&m,int(-d.ay*182.04445f)&65535);njRotateZ(&m,int(-d.az*182.04445f)&65535);njTransposeMatrix(&m);
    // At +/-90 degrees Y, X and Z are coupled. Keep a stable equivalent
    // pose instead of magnifying the PS2 sine table's tiny residual cos(Y).
    if(std::abs(m[2])>.999999f)return {0,m[2]<0?16384:49152,int(10430.381f*std::atan2(-m[4],m[5]))&65535};
    return {int(10430.381f*std::atan2(m[6],m[10]))&65535,int(10430.381f*std::asin(std::clamp(-m[2],-1.f,1.f)))&65535,int(10430.381f*std::atan2(m[1],m[0]))&65535};
}
void inspection_rotation(NJS_MATRIX& matrix,std::array<Angle,3> angles,std::array<Angle,3> spin){
    njUnitMatrix(&matrix);njRotateXYZ(&matrix,angles[0],angles[1],angles[2]);
    // DrawSubItem applies the action rotations after rotmat, in X/Y/Z order.
    njRotateX(&matrix,spin[0]);njRotateY(&matrix,spin[1]);njRotateZ(&matrix,spin[2]);
}
unsigned inspection_model_id(unsigned item){if(item>=156)throw std::runtime_error("invalid inspection item");const auto model=itemidtbl[itemflg[dsptbl[item].hide][6]];const auto id=model?model:item;return id==131?5:id==136?33:id;}
// --- Lücken ---
ModelGeometryOptions inspection_geometry(const std::vector<std::uint8_t>& packet,unsigned item,unsigned frame){
    (void)packet;(void)frame;
    if(item>=156)throw std::runtime_error("invalid inspection item");
    // Ohne MdlHideCheck/FlagErase und die drei Aktionsroutinen gibt es weder
    // eine Sichtbarkeitsmaske noch eine Aktionspose. Leere Felder bedeuten im
    // Szenendekoder "keine Überschreibung", das Paket wird also unverändert
    // gezeichnet.
    return {};
}
unsigned original_file_number(unsigned item,unsigned stage,unsigned room){(void)item;(void)stage;(void)room;return 0;}
unsigned original_file_model_mask(unsigned item){(void)item;return 0;}
unsigned original_file_pages(unsigned file){if(file>=24)throw std::runtime_error("file outside table");return 0;}
unsigned original_file_message(unsigned,unsigned){throw std::runtime_error("original file page message table is not ported (inventory_engine.inc quarantined)");}
unsigned original_file_wallpaper(unsigned){throw std::runtime_error("original file wallpaper table is not ported (inventory_engine.inc quarantined)");}
unsigned inventory_name_id(std::uint32_t item){(void)item;return 0;}
}
