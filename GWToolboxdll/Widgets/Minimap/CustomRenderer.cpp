#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/GameEntities/Hero.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Modules/Resources.h>
#include <Widgets/Minimap/CustomRenderer.h>

#define IniFilename L"Markers.ini"

using namespace std::string_literals;

void CustomRenderer::LoadSettings(CSimpleIni* ini, const char* section)
{
    color = Colors::Load(ini, section, "color_custom_markers", 0xFFFFFFFF);
    Invalidate();
    LoadMarkers();
}
void CustomRenderer::LoadMarkers()
{
    // clear current markers
    lines.clear();
    markers.clear();
    polygons.clear();

    if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
    inifile->LoadFile(Resources::GetPath(IniFilename).c_str());

    // then load new
    CSimpleIni::TNamesDepend entries;
    inifile->GetAllSections(entries);
    for (CSimpleIni::Entry& entry : entries) {
        const char* section = entry.pItem;
        if (strncmp(section, "customline", 10) == 0) {
            lines.push_back(CustomLine(inifile->GetValue(section, "name", "line")));
            lines.back().p1.x = (float)inifile->GetDoubleValue(section, "x1", 0.0);
            lines.back().p1.y = (float)inifile->GetDoubleValue(section, "y1", 0.0);
            lines.back().p2.x = (float)inifile->GetDoubleValue(section, "x2", 0.0);
            lines.back().p2.y = (float)inifile->GetDoubleValue(section, "y2", 0.0);
            lines.back().map = (GW::Constants::MapID)inifile->GetLongValue(section, "map", 0);
            lines.back().color = Colors::Load(inifile, section, "color", lines.back().color);
            lines.back().visible = inifile->GetBoolValue(section, "visible", true);
            inifile->Delete(section, nullptr);
        }
        if (strncmp(section, "custommarker", 12) == 0) {
            auto marker = CustomMarker(inifile->GetValue(section, "name", "marker"));
            marker.pos.x = static_cast<float>(inifile->GetDoubleValue(section, "x", 0.0));
            marker.pos.y = static_cast<float>(inifile->GetDoubleValue(section, "y", 0.0));
            marker.size = static_cast<float>(inifile->GetDoubleValue(section, "size", 0.0));
            marker.shape = static_cast<Shape>(inifile->GetLongValue(section, "shape", 0));
            marker.map = static_cast<GW::Constants::MapID>(inifile->GetLongValue(section, "map", 0));
            marker.color_sub = Colors::Load(inifile, section, "color_sub", marker.color_sub);
            marker.visible = inifile->GetBoolValue(section, "visible", true);
            markers.push_back(marker);
            inifile->Delete(section, nullptr);
        }
        if (strncmp(section, "custompolygon", 13) == 0) {
            auto polygon = CustomPolygon(inifile->GetValue(section, "name", "polygon"));
            for (auto i = 0; i < CustomPolygon::max_points; i++) {
                GW::Vec2f vec;
                vec.x = static_cast<float>(
                    inifile->GetDoubleValue(section, (std::string("point[") + std::to_string(i) + "].x").c_str(), 0.f));
                vec.y = static_cast<float>(
                    inifile->GetDoubleValue(section, (std::string("point[") + std::to_string(i) + "].y").c_str(), 0.f));
                if (vec.x != 0.f || vec.y != 0.f)
                    polygon.points.emplace_back(vec);
                else
                    break;
            }
            polygon.filled = inifile->GetBoolValue(section, "filled", polygon.filled);
            polygon.color = Colors::Load(inifile, section, "color", polygon.color);
            polygon.color_sub = Colors::Load(inifile, section, "color_sub", polygon.color_sub);
            polygon.map = static_cast<GW::Constants::MapID>(inifile->GetLongValue(section, "map", 0));
            polygon.visible = inifile->GetBoolValue(section, "visible", true);
            polygons.push_back(polygon);
            inifile->Delete(section, nullptr);
        }
    }

    markers_changed = false;
}
void CustomRenderer::SaveSettings(CSimpleIni* ini, const char* section) const
{
    Colors::Save(ini, section, "color_custom_markers", color);
    SaveMarkers();
}
void CustomRenderer::SaveMarkers() const
{
    // clear markers from ini
    // then load new
    if (markers_changed) {
        CSimpleIni::TNamesDepend entries;
        inifile->GetAllSections(entries);
        for (CSimpleIni::Entry& entry : entries) {
            const char* section = entry.pItem;
            if (strncmp(section, "customline", 10) == 0) {
                inifile->Delete(section, nullptr);
            }
            if (strncmp(section, "custommarker", 12) == 0) {
                inifile->Delete(section, nullptr);
            }
        }

        // then save
        for (unsigned i = 0; i < lines.size(); ++i) {
            const CustomLine& line = lines[i];
            char section[32];
            snprintf(section, 32, "customline%03d", i);
            inifile->SetValue(section, "name", line.name);
            inifile->SetDoubleValue(section, "x1", line.p1.x);
            inifile->SetDoubleValue(section, "y1", line.p1.y);
            inifile->SetDoubleValue(section, "x2", line.p2.x);
            inifile->SetDoubleValue(section, "y2", line.p2.y);
            Colors::Save(inifile, section, "color", line.color);
            inifile->SetLongValue(section, "map", (long)line.map);
            inifile->SetBoolValue(section, "visible", line.visible);
        }
        for (unsigned i = 0; i < markers.size(); ++i) {
            const CustomMarker& marker = markers[i];
            char section[32];
            snprintf(section, 32, "custommarker%03d", i);
            inifile->SetValue(section, "name", marker.name);
            inifile->SetDoubleValue(section, "x", marker.pos.x);
            inifile->SetDoubleValue(section, "y", marker.pos.y);
            inifile->SetDoubleValue(section, "size", marker.size);
            inifile->SetLongValue(section, "shape", static_cast<long>(marker.shape));
            inifile->SetLongValue(section, "map", static_cast<long>(marker.map));
            inifile->SetBoolValue(section, "visible", marker.visible);
            Colors::Save(inifile, section, "color_sub", marker.color_sub);
        }
        for (auto i = 0u; i < polygons.size(); ++i) {
            const CustomPolygon& polygon = polygons[i];
            char section[32];
            snprintf(section, 32, "custompolygon%03d", i);
            for (auto j = 0u; j < polygon.points.size(); j++) {
                inifile->SetDoubleValue(
                    section, (std::string("point[") + std::to_string(j) + "].x").c_str(), polygon.points.at(j).x);
                inifile->SetDoubleValue(
                    section, (std::string("point[") + std::to_string(j) + "].y").c_str(), polygon.points.at(j).y);
            }
            Colors::Save(inifile, section, "color", polygon.color);
            Colors::Save(inifile, section, "color_sub", polygon.color_sub);
            inifile->SetValue(section, "name", polygon.name);
            inifile->SetLongValue(section, "map", static_cast<long>(polygon.map));
            inifile->SetBoolValue(section, "visible", polygon.visible);
            inifile->SetBoolValue(section, "filled", polygon.filled);
        }

        inifile->SaveFile(Resources::GetPath(IniFilename).c_str());
    }
}
void CustomRenderer::Invalidate()
{
    VBuffer::Invalidate();
    fullcircle.Invalidate();
    linecircle.Invalidate();
}
void CustomRenderer::SetTooltipMapID(const GW::Constants::MapID& map_id) {
    if (map_id_tooltip.map_id != map_id) {
        map_id_tooltip.map_id = map_id;
        if (map_id == GW::Constants::MapID::None) {
            snprintf(map_id_tooltip.tooltip_str, sizeof(map_id_tooltip.tooltip_str), "Map ID (Any)");
        }
        else {
            snprintf(map_id_tooltip.tooltip_str, sizeof(map_id_tooltip.tooltip_str), "Map ID");
            const GW::AreaInfo* map_info = map_id < GW::Constants::MapID::Count ? GW::Map::GetMapInfo(map_id) : nullptr;
            if (map_info && map_info->name_id) {
                wchar_t map_id_buf[8];
                map_id_tooltip.map_name_ws.clear();
                if (GW::UI::UInt32ToEncStr(map_info->name_id, map_id_buf, 8))
                    GW::UI::AsyncDecodeStr(map_id_buf, &map_id_tooltip.map_name_ws);
            }
        }
    }
    if (!map_id_tooltip.map_name_ws.empty()) {
        snprintf(map_id_tooltip.tooltip_str, sizeof(map_id_tooltip.tooltip_str), "Map ID (%s)", GuiUtils::WStringToString(map_id_tooltip.map_name_ws).c_str());
        map_id_tooltip.map_name_ws.clear();
    }
    ImGui::SetTooltip(map_id_tooltip.tooltip_str);
}
void CustomRenderer::DrawLineSettings() {
    if (Colors::DrawSettingHueWheel("Color", &color)) Invalidate();
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID("lines");
    for (size_t i = 0; i < lines.size(); ++i) {
        CustomLine& line = lines[i];
        ImGui::PushID(static_cast<int>(i));
        markers_changed |= ImGui::Checkbox("##visible", &line.visible);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth((ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 5) / 5);

        markers_changed |= ImGui::DragFloat("##x1", &line.p1.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line X 1");
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##y1", &line.p1.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line Y 1");
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##x2", &line.p2.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line X 2");
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::DragFloat("##y2", &line.p2.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line Y 2");
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::ColorButtonPicker("##color", &line.color);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line Color");
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::InputInt("##map", (int*)&line.map, 0);
        if (ImGui::IsItemHovered()) SetTooltipMapID(line.map);
        ImGui::SameLine(0.0f, spacing);

        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() - spacing - 20.0f);
        markers_changed |= ImGui::InputText("##name", line.name, 128);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name");
        ImGui::SameLine(0.0f, spacing);
        bool remove = ImGui::Button("x##delete", ImVec2(20.0f, 0));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete");
        ImGui::PopID();
        if (remove) {
            lines.erase(lines.begin() + static_cast<int>(i));
            markers_changed = true;
        }
    }
    ImGui::PopID();
    if (ImGui::Button("Add Line")) {
        char buf[32];
        snprintf(buf, 32, "line%zu", lines.size());
        lines.push_back(CustomLine(buf));
        markers_changed = true;
    }
}
void CustomRenderer::DrawMarkerSettings() {
    if (Colors::DrawSettingHueWheel("Color", &color)) Invalidate();
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID("markers");
    float input_item_width = (ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 8) / 8;
    for (size_t i = 0; i < markers.size(); ++i) {
        CustomMarker& marker = markers[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Checkbox("##visible", &marker.visible);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth(input_item_width);
        markers_changed |= ImGui::DragFloat("##x", &marker.pos.x, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Marker X Position");
        ImGui::SameLine(0.0f, spacing);
        markers_changed |= ImGui::DragFloat("##y", &marker.pos.y, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Marker Y Position");
        ImGui::SameLine(0.0f, spacing);
        markers_changed |= ImGui::DragFloat("##size", &marker.size, 1.0f, 0.0f, 0.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Size");
        ImGui::SameLine(0.0f, spacing);

        static const char* const types[] = { "Circle", "FillCircle" };
        markers_changed |= ImGui::Combo("##type", reinterpret_cast<int*>(&marker.shape), types, 2);
        ImGui::SameLine(0.0f, spacing);

        ImGui::ColorButtonPicker("##colorsub", &marker.color_sub);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color in which hostile agents inside this polygon are drawn.\n\nNOTE: An alpha channel of 0 will disable this color.");
        ImGui::SameLine(0.0f, spacing);

        markers_changed |= ImGui::InputInt("##map", (int*)&marker.map, 0);
        if (ImGui::IsItemHovered()) SetTooltipMapID(marker.map);
        ImGui::SameLine(0.0f, spacing);
        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() - spacing - 20.0f);
        markers_changed |= ImGui::InputText("##name", marker.name, 128);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name");
        ImGui::SameLine(0.0f, spacing);
        bool remove = ImGui::Button("x##delete", ImVec2(20.0f, 0));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete");
        ImGui::PopID();
        if (remove) {
            markers.erase(markers.begin() + static_cast<int>(i));
            markers_changed = true;
        }
    }
    ImGui::PopID();
    if (ImGui::Button("Add Marker")) {
        char buf[32];
        snprintf(buf, 32, "marker%zu", markers.size());
        markers.push_back(CustomMarker(buf));
        markers_changed = true;
    }
}
CustomRenderer::CustomMarker::CustomMarker(const char* n) : CustomMarker(0, 0, 100.0f, Shape::LineCircle, GW::Map::GetMapID(), n) {
    auto* const player = GW::Agents::GetPlayerAsAgentLiving();
    if (player) {
        pos.x = player->pos.x;
        pos.y = player->pos.y;
    }
}
CustomRenderer::CustomPolygon::CustomPolygon(const char* n) : CustomPolygon(GW::Map::GetMapID(), n) {};
void CustomRenderer::DrawPolygonSettings() {
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID("polygons");
    float input_item_width = (ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 8) / 8;
    for (size_t i = 0; i < polygons.size(); ++i) {
        bool polygon_changed = false;
        int signed_idx = static_cast<int>(i);
        bool show_details = signed_idx == show_polygon_details;
        CustomPolygon& polygon = polygons.at(i);
        ImGui::PushID(signed_idx);
        polygon_changed |= ImGui::Checkbox("##visible", &polygon.visible);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth(input_item_width);

        if (show_details)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        char show_points_label_buf[40];
        snprintf(show_points_label_buf, sizeof(show_points_label_buf), "Show Points (%d)##show_polygon_details", polygon.points.size());
        if (ImGui::Button(show_points_label_buf, ImVec2(input_item_width * 3 + spacing * 2, 0.f))) {
            show_polygon_details = show_details ? -1 : signed_idx;
        }
        if (show_details)
            ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::Checkbox("##filled", &polygon.filled);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filled");
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::ColorButtonPicker("##colorsub", &polygon.color_sub);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color in which hostile agents inside this polygon are drawn.\n\nNOTE: An alpha channel of 0 will disable this color.");
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::ColorButtonPicker("##color", &polygon.color);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color of the polygon on the map.\nNOTE: An alpha channel of 0 will disable this color.");
        ImGui::SameLine(0.0f, spacing);

        polygon_changed |= ImGui::InputInt("##map", reinterpret_cast<int*>(&polygon.map), 0);
        if (ImGui::IsItemHovered()) SetTooltipMapID(polygon.map);
        ImGui::SameLine(0.0f, spacing);

        ImGui::PopItemWidth();
        ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() - spacing - 20.0f);
        markers_changed |= ImGui::InputText("##name", polygon.name, 128);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name");
        ImGui::PopItemWidth();
        ImGui::SameLine(0.0f, spacing);
        bool remove = ImGui::Button("x##delete", ImVec2(20.0f, 0));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete");

        if (show_details) {
            ImGui::Indent();
            if (polygon.points.size() < CustomPolygon::max_points && ImGui::Button("Add Polygon Point##add")) {
                auto* const player = GW::Agents::GetPlayerAsAgentLiving();
                if (player) {
                    polygon.points.emplace_back(player->pos.x, player->pos.y);
                    polygon_changed = true;
                }
            }
            int remove_point = -1;
            for (auto j = 0u; j < polygon.points.size(); j++) {
                polygon_changed |= ImGui::InputFloat2((std::string("##point") + std::to_string(j)).c_str(),
                    reinterpret_cast<float*>(&polygon.points.at(j)), "%.0f");
                ImGui::SameLine();
                if (ImGui::Button((std::string("x##") + std::to_string(j)).c_str()))
                    remove_point = j;
            }
            if (remove_point > -1) {
                polygon.points.erase(polygon.points.begin() + remove_point);
                polygon_changed = true;
            }
            ImGui::Unindent();
        }

        ImGui::PopID();
        if (remove) {
            polygons.erase(polygons.begin() + signed_idx);
            for (auto& poly : polygons) {
                poly.Invalidate();
            }
            markers_changed = true;
            break;
        }
        else if (polygon_changed) {
            polygon.Invalidate();
        }
        markers_changed |= polygon_changed;
    }
    ImGui::PopID();
    if (ImGui::Button("Add Polygon")) {
        char buf[32];
        snprintf(buf, 32, "polygon%zu", polygons.size());
        polygons.emplace_back(buf);
        for (auto& poly : polygons) {
            poly.Invalidate();
        }
        markers_changed = true;
    }
}
void CustomRenderer::DrawSettings()
{
    auto draw_note = []() {
        ImGui::Text(
            "Note: custom markers are stored in 'Markers.ini' in settings folder. You can share the file with other players or paste other people's markers into it.");
    };
    if (ImGui::TreeNodeEx("Custom Lines", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        draw_note();
        DrawLineSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Custom Circles", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        draw_note();
        DrawMarkerSettings();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Custom Polygons", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        draw_note();
        DrawPolygonSettings();
        ImGui::TreePop();
    }
    if (markers_changed) Invalidate();
}

void CustomRenderer::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_LINELIST;
    vertices_max = 0x100; // support for up to 256 line segments, should be enough
    vertices = nullptr;

    HRESULT hr = device->CreateVertexBuffer(
        sizeof(D3DVertex) * vertices_max, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
    if (FAILED(hr)) {
        printf("Error setting up CustomRenderer vertex buffer: HRESULT: 0x%lX\n", hr);
    }
}

void CustomRenderer::CustomPolygon::Initialize(IDirect3DDevice9* device)
{
    if (filled) {
        if (points.size() < 3) return;      // can't draw a triangle with less than 3 vertices
        type = D3DPT_TRIANGLELIST;

        
        const auto poly = std::vector<std::vector<GW::Vec2f>>{{points}};
        point_indices.clear();
        point_indices = mapbox::earcut<unsigned>(poly);

        const auto vertex_count = point_indices.size();
        D3DVertex* _vertices = nullptr;

        if (buffer) buffer->Release();
        device->CreateVertexBuffer(
            sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
        buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices), D3DLOCK_DISCARD);

        for (auto i = 0u; i < point_indices.size(); i++) {
            _vertices[i].x = points.at(point_indices.at(i)).x;
            _vertices[i].y = points.at(point_indices.at(i)).y;
            _vertices[i].z = 0.f;
            _vertices[i].color = color;
        }

        buffer->Unlock();
    } else {
        if (points.size() < 2) return;
        type = D3DPT_LINESTRIP;

        const auto vertex_count = points.size() + 1;
        D3DVertex* _vertices = nullptr;

        if (buffer) buffer->Release();
        device->CreateVertexBuffer(
            sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
        buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices), D3DLOCK_DISCARD);

        for (auto i = 0u; i < points.size(); i++) {
            _vertices[i].x = points.at(i).x;
            _vertices[i].y = points.at(i).y;
            _vertices[i].z = 0.f;
            _vertices[i].color = color;
        }

        buffer->Unlock();
    }
}

void CustomRenderer::CustomPolygon::Render(IDirect3DDevice9* device)
{
    if (!initialized) {
        initialized = true;
        Initialize(device);
    }
    if (filled) {
        if (points.size() < 3) return;
        if (visible && (map == GW::Constants::MapID::None || map == GW::Map::GetMapID())) {
            device->SetFVF(D3DFVF_CUSTOMVERTEX);
            device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
            device->DrawPrimitive(type, 0, point_indices.size() / 3);
        }
    } else {
        if (points.size() < 2) return;
        if (visible && (map == GW::Constants::MapID::None || map == GW::Map::GetMapID())) {
            device->SetFVF(D3DFVF_CUSTOMVERTEX);
            device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
            device->DrawPrimitive(type, 0, points.size() - 1);
        }
    }
}

void CustomRenderer::FullCircle::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLEFAN;
    count = 48; // polycount
    unsigned int vertex_count = count + 2;
    D3DVertex* _vertices = nullptr;

    if (buffer) buffer->Release();
    device->CreateVertexBuffer(
        sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count, (VOID**)&_vertices, D3DLOCK_DISCARD);

    const float PI = 3.1415927f;
    _vertices[0].x = 0.0f;
    _vertices[0].y = 0.0f;
    _vertices[0].z = 0.0f;
    _vertices[0].color = Colors::Sub(color, Colors::ARGB(50, 0, 0, 0));
    for (size_t i = 1; i < vertex_count; ++i) {
        float angle = (i - 1) * (2 * PI / count);
        _vertices[i].x = std::cos(angle);
        _vertices[i].y = std::sin(angle);
        _vertices[i].z = 0.0f;
        _vertices[i].color = color;
    }

    buffer->Unlock();
}

void CustomRenderer::LineCircle::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_LINESTRIP;
    count = 48; // polycount
    unsigned int vertex_count = count + 1;
    D3DVertex* _vertices = nullptr;

    if (buffer) buffer->Release();
    device->CreateVertexBuffer(
        sizeof(D3DVertex) * vertex_count, 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count, (VOID**)&_vertices, D3DLOCK_DISCARD);

    for (size_t i = 0; i < count; ++i) {
        float angle = i * (2 * static_cast<float>(M_PI) / (count + 1));
        _vertices[i].x = std::cos(angle);
        _vertices[i].y = std::sin(angle);
        _vertices[i].z = 0.0f;
        _vertices[i].color = color; // 0xFF666677;
    }
    _vertices[count] = _vertices[0];

    buffer->Unlock();
}

void CustomRenderer::Render(IDirect3DDevice9* device)
{
    if (!initialized) {
        initialized = true;
        Initialize(device);
    }

    DrawCustomMarkers(device);

    vertices_count = 0;
    HRESULT res = buffer->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_DISCARD);
    if (FAILED(res)) printf("PingsLinesRenderer Lock() error: HRESULT: 0x%lX\n", res);

    DrawCustomLines(device);

    D3DXMATRIX i;
    D3DXMatrixIdentity(&i);
    device->SetTransform(D3DTS_WORLD, &i);

    buffer->Unlock();
    if (vertices_count != 0) {
        device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
        device->DrawPrimitive(type, 0, vertices_count / 2);
        vertices_count = 0;
    }
}

void CustomRenderer::DrawCustomMarkers(IDirect3DDevice9* device)
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        for (CustomPolygon& polygon : polygons) {
            polygon.Render(device);
        }

        for (const CustomMarker& marker : markers) {
            if (marker.visible && (marker.map == GW::Constants::MapID::None || marker.map == GW::Map::GetMapID())) {
                D3DXMATRIX translate, scale, world;
                D3DXMatrixTranslation(&translate, marker.pos.x, marker.pos.y, 0.0f);
                D3DXMatrixScaling(&scale, marker.size, marker.size, 1.0f);
                world = scale * translate;
                device->SetTransform(D3DTS_WORLD, &world);

                switch (marker.shape) {
                    case Shape::LineCircle:
                        linecircle.Render(device);
                        break;
                    case Shape::FullCircle:
                        fullcircle.Render(device);
                        break;
                }
            }
        }

        GW::HeroFlagArray& flags = GW::GameContext::instance()->world->hero_flags;
        if (flags.valid()) {
            for (unsigned i = 0; i < flags.size(); ++i) {
                GW::HeroFlag& flag = flags[i];
                D3DXMATRIX translate, scale, world;
                D3DXMatrixTranslation(&translate, flag.flag.x, flag.flag.y, 0.0f);
                D3DXMatrixScaling(&scale, 200.0f, 200.0f, 1.0f);
                world = scale * translate;
                device->SetTransform(D3DTS_WORLD, &world);
                linecircle.Render(device);
            }
        }
        GW::Vec3f allflag = GW::GameContext::instance()->world->all_flag;
        D3DXMATRIX translate, scale, world;
        D3DXMatrixTranslation(&translate, allflag.x, allflag.y, 0.0f);
        D3DXMatrixScaling(&scale, 300.0f, 300.0f, 1.0f);
        world = scale * translate;
        device->SetTransform(D3DTS_WORLD, &world);
        linecircle.Render(device);
    }
}

void CustomRenderer::DrawCustomLines(IDirect3DDevice9* device)
{
    UNREFERENCED_PARAMETER(device);
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        for (const CustomLine& line : lines) {
            if (line.visible && (line.map == GW::Constants::MapID::None || line.map == GW::Map::GetMapID())) {
                EnqueueVertex(line.p1.x, line.p1.y, line.color);
                EnqueueVertex(line.p2.x, line.p2.y, line.color);
            }
        }
    }
}

void CustomRenderer::EnqueueVertex(float x, float y, Color _color)
{
    if (vertices_count == vertices_max) return;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[0].color = _color;
    ++vertices;
    ++vertices_count;
}
