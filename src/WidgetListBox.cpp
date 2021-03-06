/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Justin Jacobs

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * class WidgetListBox
 */

#include "WidgetListBox.h"
#include "SharedResources.h"
#include "SDL_gfxBlitFunc.h"

using namespace std;

WidgetListBox::WidgetListBox(int amount, int height, const std::string& _fileName)
	: fileName(_fileName) {

	list_amount = amount;
	list_height = height;
	cursor = 0;
	has_scroll_bar = false;
	non_empty_slots = 0;
	values = new std::string[list_amount];
	tooltips = new std::string[list_amount];
	vlabels = new WidgetLabel[list_height];
	rows = new SDL_Rect[list_height];
	tip = new WidgetTooltip();

	listboxs = NULL;
	click = NULL;
	pos.x = pos.y = pos.w = pos.h = 0;

	selected = new bool[list_amount];
	for (int i=0; i<list_amount; i++) {
		selected[i] = false;
		values[i] = "";
	}

	multi_select = false;
	can_deselect = true;
	
	loadArt();

	pos.w = listboxs->w;
	pos.h = (listboxs->h / 3); //height of one item

	scrollbar = new WidgetScrollBar(mods->locate("images/menus/buttons/scrollbar_default.png"));

	render_to_alpha = false;

	color_normal = font->getColor("widget_normal");
	color_disabled = font->getColor("widget_disabled");
}

void WidgetListBox::loadArt() {

	// load ListBox images
	listboxs = IMG_Load(fileName.c_str());

	if(!listboxs) {
		fprintf(stderr, "Couldn't load image: %s\n", IMG_GetError());
		SDL_Quit();
		exit(1); // or abort ??
	}
	
	// optimize
	SDL_Surface *cleanup = listboxs;
	listboxs = SDL_DisplayFormatAlpha(listboxs);
	SDL_FreeSurface(cleanup);
}

bool WidgetListBox::checkClick() {
	if (checkClick(inpt->mouse.x,inpt->mouse.y))
		return true;
	else
		return false;
}

/**
 * Sets and releases the "pressed" visual state of the ListBox
 * If press and release, activate (return true)
 */
bool WidgetListBox::checkClick(int x, int y) {
	Point mouse = {x,y};

	refresh();
	tip_new = checkTooltip(mouse);

	// check ScrollBar clicks
	if (has_scroll_bar) {
		switch (scrollbar->checkClick(mouse.x,mouse.y)) {
			case 1:
				scrollUp();
				break;
			case 2:
				scrollDown();
				break;
            case 3:
                cursor = scrollbar->getValue();
                refresh();
                break;
			default:
				break;
		}
	}

	// main ListBox already in use, new click not allowed
	if (inpt->lock[MAIN1]) return false;

	// main click released, so the ListBox state goes back to unpressed
	if (pressed && !inpt->lock[MAIN1]) {
		pressed = false;
		
		for(int i=0; i<list_height; i++) {
			if (i<list_amount) {
				if (isWithin(rows[i], mouse) && values[i+cursor] != "") {
					// deselect other options if multi-select is disabled
					if (!multi_select) {
						for (int j=0; j<list_amount; j++) {
							if (j!=i+cursor)
								selected[j] = false;
						}
					}
					// activate upon release
					if (selected[i+cursor]) {
						if (can_deselect) selected[i+cursor] = false;
					} else {
						selected[i+cursor] = true;
					}
					refresh();
					return true;
				}
			}
		}
	}

	pressed = false;

	// detect new click
	if (inpt->pressing[MAIN1]) {
		for (int i=0; i<list_height;i++) {
			if (isWithin(rows[i], mouse)) {
			
				inpt->lock[MAIN1] = true;
				pressed = true;

			}
		}
	}
	return false;

}

/**
 * If mousing-over an item with a tooltip, return that tooltip data.
 *
 * @param mouse The x,y screen coordinates of the mouse cursor
 */
TooltipData WidgetListBox::checkTooltip(Point mouse) {
	TooltipData _tip;

	for(int i=0; i<list_height; i++) {
		if (i<list_amount) {
			if (isWithin(rows[i], mouse) && tooltips[i+cursor] != "") {
				_tip.lines[_tip.num_lines++] = tooltips[i+cursor];
				break;
			}
		}
	}

	return _tip;
}

/**
 * Set the value and tooltip of the first available slot
 */
void WidgetListBox::append(std::string value, std::string tooltip) {
	for (int i=0;i<list_amount;i++) {
		if (values[i] == "") {
			values[i] = value;
			tooltips[i] = tooltip;
			refresh();
			return;
		}
	}
}

/**
 * Clear a slot at a specified index, shifting the other items accordingly
 */
void WidgetListBox::remove(int index) {
	for (int i=index;i<list_amount;i++) {
		if (i==list_amount-1) {
			selected[i] = false;
			values[i] = "";
			tooltips[i] = "";
		} else {
			selected[i] = selected[i+1];
			values[i] = values[i+1];
			tooltips[i] = tooltips[i+1];
		}
	}
	scrollUp();
	refresh();
}

/*
 * Clear the list
 */
void WidgetListBox::clear() {
	for (int i=0; i<list_amount; i++) {
		selected[i] = false;
		values[i] = "";
		tooltips[i] = "";
	}
	refresh();
}

/*
 * Move an item up on the list
 */
void WidgetListBox::shiftUp() {
	any_selected = false;
	if (!selected[0]) {
		for (int i=1; i < non_empty_slots; i++) {
			if (selected[i]) {
				any_selected = true;
				bool tmp_selected = selected[i];
				std::string tmp_value = values[i];
				std::string tmp_tooltip = tooltips[i];

				selected[i] = selected[i-1];
				values[i] = values[i-1];
				tooltips[i] = tooltips[i-1];

				selected[i-1] = tmp_selected;
				values[i-1] = tmp_value;
				tooltips[i-1] = tmp_tooltip;
			}
		}
		if (any_selected) {
			scrollUp();
		}
	}
}

/*
 * Move an item down on the list
 */
void WidgetListBox::shiftDown() {
	any_selected = false;
	if (!selected[non_empty_slots-1]) {
		for (int i=non_empty_slots-2; i >= 0; i--) {
			if (selected[i]) {
				any_selected = true;
				bool tmp_selected = selected[i];
				std::string tmp_value = values[i];
				std::string tmp_tooltip = tooltips[i];

				selected[i] = selected[i+1];
				values[i] = values[i+1];
				tooltips[i] = tooltips[i+1];

				selected[i+1] = tmp_selected;
				values[i+1] = tmp_value;
				tooltips[i+1] = tmp_tooltip;
			}
		}
		if (any_selected) {
			scrollDown();
		}
	}
}

std::string WidgetListBox::getValue() {
	for (int i=0; i<non_empty_slots; i++) {
		if (selected[i]) return values[i];
	}
	return "";
}

/*
 * Get the item name at a specific index
 */
std::string WidgetListBox::getValue(int index) {
	return values[index];
}

/*
 * Get the item tooltip at a specific index
 */
std::string WidgetListBox::getTooltip(int index) {
	return tooltips[index];
}

/*
 * Get the amount of ListBox items
 */
int WidgetListBox::getSize() {
	return list_amount;
}

/*
 * Shift the viewing area up
 */
void WidgetListBox::scrollUp() {
	if (cursor > 0)
		cursor -= 1;
	refresh();
}

/*
 * Shift the viewing area down
 */
void WidgetListBox::scrollDown() {
	if (cursor+list_height < non_empty_slots)
		cursor += 1;
	refresh();
}

void WidgetListBox::render(SDL_Surface *target) {
	if (target == NULL) {
		target = screen;
	}

	SDL_Rect src;
	src.x = 0;
	src.w = pos.w;
	src.h = pos.h;

	for(int i=0; i<list_height; i++) {
		if(i==0)
			src.y = 0;
		else if(i==list_height-1)
			src.y = pos.h*2;
		else
			src.y = pos.h;

		if (render_to_alpha)
			SDL_gfxBlitRGBA(listboxs, &src, target, &rows[i]);
		else
			SDL_BlitSurface(listboxs, &src, target, &rows[i]);
		if (i<list_amount) {
			vlabels[i].render(target);
		}
	}

	if (has_scroll_bar)
		scrollbar->render(target);

	if (tip_new.num_lines > 0) {
		if (tip_new.lines[0] != tip_buf.lines[0]) {
			tip_buf.clear();
			tip_buf = tip_new;
		}
		tip->render(tip_buf, inpt->mouse, STYLE_FLOAT, target);
	}


}

/**
 * Create the text buffer
 * Also, toggle the scrollbar based on the size of the list
 */
void WidgetListBox::refresh() {

	std::string temp;

	// Get the number of slots that have content
	non_empty_slots = 0;
	for (int i=0;i<list_amount;i++) {
		if (values[i] != "")
			non_empty_slots = i+1;
	}

	// Update the scrollbar
	if (non_empty_slots > list_height) {
		has_scroll_bar = true;
		pos_scroll.x = pos.x+pos.w-3-scrollbar->pos_up.w;
		pos_scroll.y = pos.y+3;
		pos_scroll.w = scrollbar->pos_up.w;
		pos_scroll.h = ((pos.h-1)*list_height)-scrollbar->pos_down.h-7;
		scrollbar->refresh(pos_scroll.x, pos_scroll.y, pos_scroll.h, cursor, non_empty_slots-list_height);
	} else {
		has_scroll_bar = false;
	}

	// Update each row's hitbox and label
	for(int i=0;i<list_height;i++)
	{
		rows[i].x = pos.x;
		rows[i].y = ((pos.h-1)*i)+pos.y;
		if (has_scroll_bar) {
			rows[i].w = pos.w - pos_scroll.w;
		} else {
			rows[i].w = pos.w;
		}
		rows[i].h = pos.h;

		int font_x = rows[i].x + 8;
		int font_y = rows[i].y + (rows[i].h/2);

		if (i<list_amount) {
			if (values[i+cursor].length() > (float)pos.w/5.3) {
				temp = values[i+cursor].substr(0,23);
				temp.append("...");
			} else {
				temp = values[i+cursor];
			}

			if(selected[i+cursor]) {
				vlabels[i].set(font_x, font_y, JUSTIFY_LEFT, VALIGN_CENTER, temp, color_normal);
			} else {
				vlabels[i].set(font_x, font_y, JUSTIFY_LEFT, VALIGN_CENTER, temp, color_disabled);
			}
		}
	}

}

WidgetListBox::~WidgetListBox() {
	tip_buf.clear();
	SDL_FreeSurface(listboxs);
	delete[] values;
	delete[] tooltips;
	delete[] vlabels;
	delete[] rows;
	delete[] selected;
	delete tip;
	delete scrollbar;
}

