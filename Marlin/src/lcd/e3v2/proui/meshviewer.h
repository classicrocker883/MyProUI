/**
 * Mesh Viewer for PRO UI
 * Author: Miguel A. Risco-Castillo (MRISCOC)
 * version: 4.2.1
 * Date: 2023/05/05
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

class MeshViewerClass {
public:
  const uint8_t meshfont = TERN(TJC_DISPLAY, font8x16, font6x12);
  static float max, min;
  static void DrawMeshGrid(const uint8_t csizex, const uint8_t csizey);
  static void DrawMeshPoint(const uint8_t x, const uint8_t y, const float z);
  static void Draw(const bool withsave=false, const bool redraw=true);
  static void DrawMesh(const bed_mesh_t zval, const uint8_t csizex, const uint8_t csizey);
};

extern MeshViewerClass MeshViewer;

void Goto_MeshViewer(const bool redraw);
