/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2016  Christian Fillion
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "listview.hpp"

#ifdef _WIN32
#include <commctrl.h>
#endif

using namespace std;

ListView::ListView(const Columns &columns, HWND handle)
  : Control(handle), m_columnSize(0),
    m_sortColumn(-1), m_sortOrder(AscendingOrder)
{
  for(const Column &col : columns)
    addColumn(col);

  setExStyle(LVS_EX_FULLROWSELECT, true);

#ifdef LVS_EX_LABELTIP
  // unsupported by SWELL, but always enabled on OS X anyway
  setExStyle(LVS_EX_LABELTIP, true);
#endif
}

void ListView::setExStyle(const int style, const bool enable)
{
  ListView_SetExtendedListViewStyleEx(handle(), style, enable ? style : 0);
}

void ListView::addColumn(const Column &col)
{
  LVCOLUMN item{};

  item.mask |= LVCF_WIDTH;
  item.cx = col.width;

  item.mask |= LVCF_TEXT;
  item.pszText = const_cast<auto_char *>(col.text.c_str());

  ListView_InsertColumn(handle(), m_columnSize++, &item);
}

int ListView::addRow(const Row &content)
{
  LVITEM item{};
  item.iItem = rowCount();

  item.mask |= LVIF_PARAM;
  item.lParam = item.iItem;

  ListView_InsertItem(handle(), &item);

  m_rows.resize(item.iItem + 1); // make room for the new row
  replaceRow(item.iItem, content);

  return item.iItem;
}

void ListView::replaceRow(const int index, const Row &content)
{
  m_rows[index] = content;

  const int cols = min(m_columnSize, (int)content.size());

  for(int i = 0; i < cols; i++) {
    auto_char *text = const_cast<auto_char *>(content[i].c_str());
    ListView_SetItemText(handle(), index, i, text);
  }
}

void ListView::removeRow(const int index)
{
  ListView_DeleteItem(handle(), index);
  m_rows.erase(m_rows.begin() + index);

  // shift lParam to reflect the new row indexes
  const int size = rowCount();
  for(int i = index; i < size; i++) {
    LVITEM item{};
    item.iItem = i;
    item.mask |= LVIF_PARAM;
    item.lParam = i;
    ListView_SetItem(handle(), &item);
  }
}

void ListView::resizeColumn(const int index, const int width)
{
  ListView_SetColumnWidth(handle(), index, width);
}

void ListView::sort()
{
  if(m_sortColumn > -1)
    sortByColumn(m_sortColumn, m_sortOrder);
}

void ListView::sortByColumn(const int index, const SortOrder order)
{
  static const auto compare = [](LPARAM aRow, LPARAM bRow, LPARAM param)
  {
    ListView *view = reinterpret_cast<ListView *>(param);
    const int column = view->m_sortColumn;

    const auto_string &a = view->m_rows[aRow][column];
    const auto_string &b = view->m_rows[bRow][column];

    const int ret = a.compare(b);

    switch(view->m_sortOrder) {
    case AscendingOrder:
      return ret;
    case DescendingOrder:
    default: // for MSVC
      return -ret;
    }
  };

  if(m_sortColumn > -1)
    setSortArrow(false);

  m_sortColumn = index;
  m_sortOrder = order;
  setSortArrow(true);

  ListView_SortItems(handle(), compare, (LPARAM)this);
}

void ListView::setSortArrow(const bool set)
{
  HWND header = ListView_GetHeader(handle());

  HDITEM item{};
  item.mask |= HDI_FORMAT;

  if(!Header_GetItem(header, m_sortColumn, &item))
    return;

  item.fmt &= ~(HDF_SORTDOWN | HDF_SORTUP); // clear

  if(set) {
    switch(m_sortOrder) {
    case AscendingOrder:
      item.fmt |= HDF_SORTUP;
      break;
    case DescendingOrder:
      item.fmt |= HDF_SORTDOWN;
    }
  }

  Header_SetItem(header, m_sortColumn, &item);
}

void ListView::clear()
{
  ListView_DeleteAllItems(handle());

  m_rows.clear();
}

bool ListView::hasSelection() const
{
  return currentIndex() > -1;
}

int ListView::currentIndex() const
{
  const int internalIndex = ListView_GetNextItem(handle(), -1, LVNI_SELECTED);

  if(internalIndex < 0)
    return -1;

  LVITEM item{};
  item.iItem = internalIndex;
  item.mask |= LVIF_PARAM;
  ListView_GetItem(handle(), &item);

  return (int)item.lParam;
}

void ListView::onNotify(LPNMHDR info, LPARAM lParam)
{
  switch(info->code) {
  case LVN_ITEMCHANGED:
    m_onSelect();
    break;
  case NM_DBLCLK:
    m_onDoubleClick();
    break;
  case LVN_COLUMNCLICK:
    onColumnClick(lParam);
    break;
  };
}

void ListView::onColumnClick(LPARAM lParam)
{
  auto info = (LPNMLISTVIEW)lParam;
  const int col = info->iSubItem;
  SortOrder order = AscendingOrder;

  if(col == m_sortColumn) {
    switch(m_sortOrder) {
    case AscendingOrder:
      order = DescendingOrder;
      break;
    case DescendingOrder:
      order = AscendingOrder;
      break;
    }
  }

  sortByColumn(col, order);
}
