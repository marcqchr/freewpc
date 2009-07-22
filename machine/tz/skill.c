/*
 * Copyright 2006, 2007, 2008, 2009 by Brian Dominy <brian@oddchange.com>
 *
 * This file is part of FreeWPC.
 *
 * FreeWPC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * FreeWPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with FreeWPC; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <freewpc.h>

bool skill_shot_enabled;

U8 skill_switch_reached;

__local__ U8 skill_min_value;


extern inline void flash_deff_begin_static (void)
{
	dmd_alloc_low_high ();
	dmd_clean_page_low ();
}

extern inline void flash_deff_begin_flashing (void)
{
	dmd_copy_low_to_high ();
}

extern inline __noreturn__ void flash_deff_run (void)
{
	dmd_show_low ();
	for (;;)
	{
		task_sleep (TIME_100MS * 3); /* all text on */
		dmd_show_other ();
		task_sleep (TIME_100MS * 1); /* flashing text on */
		dmd_show_other ();
	}
}


void skill_shot_ready_deff (void)
{
	flash_deff_begin_static ();

	sprintf ("PLAYER %d", player_up);
	font_render_string_left (&font_var5, 1, 2, sprintf_buffer);

	sprintf_current_score ();
	font_render_string_right (&font_mono5, 127, 2, sprintf_buffer);

	font_render_string (&font_mono5, 16, 10, "YELLOW");
	font_render_string (&font_mono5, 16, 16, "ORANGE");
	font_render_string (&font_mono5, 16, 22, "RED");

	flash_deff_begin_flashing ();

	sprintf ("%d,000,000", skill_min_value+3);
	font_render_string_right (&font_mono5, 110, 10, sprintf_buffer);
	sprintf ("%d,000,000", skill_min_value+1);
	font_render_string_right (&font_mono5, 110, 16, sprintf_buffer);
	sprintf ("%d,000,000", skill_min_value);
	font_render_string_right (&font_mono5, 110, 22, sprintf_buffer);

	flash_deff_run ();
}


void enable_skill_shot (void)
{
	skill_shot_enabled = TRUE;
	skill_switch_reached = 0;
	deff_start (DEFF_SKILL_SHOT_READY);
}

void disable_skill_shot (void)
{
	skill_shot_enabled = FALSE;
	deff_stop (DEFF_SKILL_SHOT_READY);
}

void skill_shot_made_deff (void)
{
	dmd_alloc_low_clean ();
	dmd_sched_transition (&trans_scroll_up);
	font_render_string_center (&font_fixed10, 64, 8, "SKILL SHOT");
	switch (skill_switch_reached)
	{
		case 1:
			sprintf ("%d,000,000", skill_min_value);
			break;
		case 2:
			sprintf ("%d,000,000", skill_min_value+1);
			break;
		case 3:
			sprintf ("%d,000,000", skill_min_value+3);
			break;
	}
	font_render_string_center (&font_times8, 64, 23, sprintf_buffer);
	dmd_show_low ();
	task_sleep_sec (1);
	dmd_sched_transition (&trans_scroll_down);
	deff_exit ();
}


static void award_skill_shot (void)
{
	set_valid_playfield ();
	deff_start (DEFF_SKILL_SHOT_MADE);
	task_sleep (TIME_66MS);
	leff_restart (LEFF_FLASHER_HAPPY);
	sound_send (SND_SKILL_SHOT_CRASH_1);
	disable_skill_shot ();
	switch (skill_switch_reached)
	{
		case 1:
			callset_invoke (skill_red);
			score_1M (skill_min_value);
			break;
		case 2: 
			callset_invoke (skill_orange);
			score_1M (skill_min_value+1);
			skill_min_value++;
			timed_game_extend (5);
			break;
		case 3: 
			callset_invoke (skill_yellow);
			score_1M (skill_min_value+3);
			skill_min_value += 2;
			timed_game_extend (10);
			break;
	}
	if (skill_min_value > 7)
		skill_min_value = 7;
}

static void skill_switch_monitor (void)
{
	if (skill_switch_reached < 3)
		task_sleep_sec (1);
	else
		task_sleep_sec (3);
	award_skill_shot ();
	task_exit ();
}


static void award_skill_switch (U8 sw)
{
	callset_invoke (any_skill_switch);
	event_can_follow (any_skill_switch, slot, TIME_3S);
	if (!skill_shot_enabled && !flag_test (FLAG_SSSMB_RUNNING))
		return;

	if (skill_switch_reached < sw)
	{
		skill_switch_reached = sw;
		task_recreate_gid (GID_SKILL_SWITCH_TRIGGER, skill_switch_monitor);
		sound_send (skill_switch_reached + SND_SKILL_SHOT_RED);
	}
	else
	{
		task_kill_gid (GID_SKILL_SWITCH_TRIGGER);
		award_skill_shot ();
	}
}


CALLSET_ENTRY (skill, sw_skill_bottom)
{
	award_skill_switch (1);
}


CALLSET_ENTRY (skill, sw_skill_center)
{
	award_skill_switch (2);
}


CALLSET_ENTRY (skill, sw_skill_top)
{
	award_skill_switch (3);
}


CALLSET_ENTRY (skill, start_player)
{
	skill_min_value = 1;
}


CALLSET_ENTRY (skill, start_ball)
{
	enable_skill_shot ();
}


CALLSET_ENTRY (skill, valid_playfield)
{
	disable_skill_shot ();
}


CALLSET_ENTRY (skill, end_game)
{
	disable_skill_shot ();
}

CALLSET_ENTRY (skill, sw_shooter)
{
	/* Because the shooter switch is declared as an 'edge' switch,
	an event is generated on both transitions.  Check the current
	state of the switch to see which transition occurred.
	TODO : genmachine can be made to generate two events here,
	say 'sw_shooter_active' and 'sw_shooter_inactive', which would
	eliminate the need for every handler to check this. */
	if (!switch_poll_logical (SW_SHOOTER))
	{
		if (skill_shot_enabled
			&& !timer_find_gid (GID_SHOOTER_SOUND_DEBOUNCE))
		{
			sound_send (SND_SHOOTER_PULL);
			leff_restart (LEFF_STROBE_UP);
			timer_restart_free (GID_SHOOTER_SOUND_DEBOUNCE, TIME_3S);
		}
	}
	else
	{
		leff_stop (LEFF_STROBE_UP);
	}
}

