#include "times_history_container.h"

#include "../simhalt.h"
#include "../simline.h"
#include "../dataobj/schedule.h"
#include "../display/viewport.h"

class karte_ptr_t times_history_container_t::welt;

times_history_container_t::times_history_container_t(const player_t *owner_, schedule_t *schedule_, times_history_map *map_, bool mirrored_, bool reversed_):
	owner(owner_),
	schedule(schedule_),
	map(map_),
	mirrored(mirrored_),
	reversed(reversed_),
	lbl_order(NULL),
	lbl_name_header("Stations"),
	lbl_time_header("Latest 3"),
	lbl_average_header("Average")
{
	lbl_average_header.set_align(gui_label_t::centered);
	lbl_time_header.set_align(gui_label_t::centered);
	lbl_name_header.set_align(gui_label_t::centered);

	add_component(&lbl_order);
	add_component(&lbl_name_header);
	add_component(&lbl_time_header);
	add_component(&lbl_average_header);

	update_container();
}

times_history_container_t::~times_history_container_t() {
	delete_components();
}

void times_history_container_t::delete_components() {
	while (!buttons.empty()) {
		button_t *b = buttons.remove_first();
		remove_component(b);
		delete b;
	}
	while (!name_labels.empty()) {
		gui_label_t *l = name_labels.remove_first();
		remove_component(l);
		delete[] l->get_text_pointer();
		delete l;
	}
	while (!entry_labels.empty()) {
		times_history_entry_t *l = entry_labels.remove_first();
		remove_component(l);
		delete l;
	}
}

void times_history_container_t::update_container() {
	delete_components();

	// Create list of entries displayed.
	// This depends on mirror/reverse states of line/convoi
	const minivec_tpl<schedule_entry_t>& entries = schedule->entries;
	const uint8 size = entries.get_count();

	if (size <= 1) return;

	slist_tpl<uint8> *schedule_indices = new slist_tpl<uint8>();
	slist_tpl<departure_point_t *> *time_keys = new slist_tpl<departure_point_t *>();

	if (mirrored) {
		// Mirrored direction
		lbl_order.set_text("Bidirectional:");

		sint16 first_halt_index = -1;
		sint16 last_halt_index = -1;

		for (sint16 i = 0; i < size; i++) {
			const halthandle_t halt = haltestelle_t::get_halt(entries[i].pos, owner);
			if (halt.is_bound()) {
				if (first_halt_index == -1 || i < first_halt_index) first_halt_index = i;
				if (last_halt_index == -1 || i > last_halt_index) last_halt_index = i;
				schedule_indices->append(i);
				if (schedule_indices->get_count() > 1) {
					time_keys->append(new departure_point_t(i - 1, false));
				}
			}
		}
		for (sint16 i = last_halt_index - 1; i >= 0; i--) {
			const halthandle_t halt = haltestelle_t::get_halt(entries[i].pos, owner);
			if (halt.is_bound()) {
				schedule_indices->append(i);
				time_keys->append(new departure_point_t(i + 1, true));
			}
		}
	}
	else if (reversed) {
		// Reversed direction
		lbl_order.set_text("Reversed order:");
		sint16 first_halt_index = -1;
		for (sint16 i = size - 1; i >= 0; i--) {
			const halthandle_t halt = haltestelle_t::get_halt(entries[i].pos, owner);
			if (halt.is_bound()) {
				if (first_halt_index == -1 || i > first_halt_index) first_halt_index = i;
				schedule_indices->append(i);
				if (schedule_indices->get_count() > 1) {
					time_keys->append(new departure_point_t(i + 1, true));
				}
			}
		}
		schedule_indices->append(first_halt_index);
		time_keys->append(&departure_point_t((first_halt_index + 1) % size, true));
	}
	else {
		// Normal direction
		lbl_order.set_text("Normal order:");
		sint16 first_halt_index = -1;
		for (sint16 i = 0; i < size; i++) {
			const halthandle_t halt = haltestelle_t::get_halt(entries[i].pos, owner);
			if (halt.is_bound()) {
				if (first_halt_index == -1 || i < first_halt_index) first_halt_index = i;
				schedule_indices->append(i);
				if (schedule_indices->get_count() > 1) {
					time_keys->append(new departure_point_t(i - 1, false));
				}
			}
		}
		schedule_indices->append(first_halt_index);
		time_keys->append(&departure_point_t((first_halt_index - 1 + size) % size, false));
	}

	scr_coord_val y = LINESPACE + D_V_SPACE + LINESPACE + D_V_SPACE;
	for (int i = 0; i < schedule_indices->get_count(); i++) {
		const schedule_entry_t entry = entries[schedule_indices->at(i)];
		const halthandle_t halt = haltestelle_t::get_halt(entry.pos, owner);

		button_t *b = new button_t();
		b->init(button_t::posbutton, NULL, scr_coord(0, y));
		b->set_targetpos(entry.pos.get_2d());
		b->add_listener(this);
		add_component(b);
		buttons.append(b);

		gui_label_t *name_label = new gui_label_t(NULL);
		char *halt_name = new char[strlen(halt->get_name()) + 1];
		strcpy(halt_name, halt->get_name());
		name_label->set_text_pointer(halt_name);
		name_label->set_pos(scr_coord(D_POS_BUTTON_WIDTH + D_H_SPACE, y));
		add_component(name_label);
		name_labels.append(name_label);

		if (i < schedule_indices->get_count() - 1) {
			times_history_data_t value;
			departure_point_t t = *time_keys->at(i);
			const times_history_data_t *retrieved_value = map->access(*time_keys->at(i));
			if (retrieved_value) value = *retrieved_value;

			times_history_entry_t *entry_label = new times_history_entry_t(&value);
			add_component(entry_label);
			entry_labels.append(entry_label);
		}

		y += 2 * LINESPACE;
	}

	this->size.h = y - LINESPACE;

	delete schedule_indices;
	delete time_keys;
}

void times_history_container_t::draw(scr_coord offset) {
	const scr_size size = get_size();
	const scr_coord_val width = size.w;
	const scr_coord_val time_column_x = width - TIME_TEXT_MARGIN - TIME_TEXT_WIDTH * (TIMES_HISTORY_SIZE + 1);
	scr_coord_val y = 0;

	lbl_order.set_pos(scr_coord(0, y));
	y += LINESPACE + D_V_SPACE;

	lbl_name_header.set_pos(scr_coord(0, y));
	lbl_name_header.set_width(time_column_x);
	lbl_time_header.set_pos(scr_coord(time_column_x, y));
	lbl_time_header.set_width(TIME_TEXT_WIDTH * TIMES_HISTORY_SIZE);
	lbl_average_header.set_pos(scr_coord(time_column_x + TIME_TEXT_WIDTH * TIMES_HISTORY_SIZE + TIME_TEXT_MARGIN, y));
	lbl_average_header.set_width(TIME_TEXT_WIDTH);
	y += LINESPACE + D_V_SPACE;

	for (int i = 0; i < name_labels.get_count(); i++) {
		gui_label_t *name_label = name_labels.at(i);
		name_label->set_width(time_column_x - D_POS_BUTTON_WIDTH - D_H_SPACE);
	}
	for (int i = 0; i < entry_labels.get_count(); i++) {
		times_history_entry_t *entry_label = entry_labels.at(i);
		entry_label->set_pos(scr_coord(time_column_x, y + LINESPACE * (2 * i + 1)));
	}

	gui_container_t::draw(offset);
}

bool times_history_container_t::action_triggered(gui_action_creator_t *comp, value_t extra)
{
	if (extra.i & ~1) {
		koord k = *(const koord *)extra.p;
		welt->get_viewport()->change_world_position(koord3d(k, welt->max_hgt(k)));
	}

	return true;
}