/*
 * bingo.c - Simple bingo drawer
 *
 * Copyright (c) 2017 - 2025	Andrew Clayton <ac@sigsegv.uk>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#define _POSIX_C_SOURCE	200112L		/* clock_gettime(2) */
#define _XOPEN_SOURCE	500		/* srandom(3), random(3) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <glib.h>

#include <gtk/gtk.h>

#include <libac.h>

#include "sayings.h"

#define NR_NUMBERS	90
#define DELAY		 7

enum player_state { NOT_RUNNING = 0, RUNNING, PAUSED };

struct widgets {
	GtkWidget *window;

	GtkWidget *start;
	GtkWidget *reset;
	GtkWidget *quit;

	GtkWidget *status;
	GtkWidget *saying;
	GtkWidget *random;
	GtkWidget *previous_numbers;
	GtkWidget *ordered_numbers;

	GtkTextBuffer *pbuf;
	GtkTextBuffer *obuf;
	GtkTextBuffer *sbuf;
};

static ac_slist_t *list;

static struct widgets *w;
static timer_t bingo_timer;

static unsigned short ordered_numbers[NR_NUMBERS];
static int numbers_remaining;
static char prev_numbers[512];

static int state;

static void set_timer(int secs)
{
	struct itimerspec its;

	its.it_value.tv_sec = secs;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;

	timer_settime(bingo_timer, 0, &its, NULL);
}

#define V_BAR	"▏"	/* U+258F LEFT ONE EIGHTH BLOCK */
static void do_bingo(int sig __attribute__((unused)))
{
	int i;
	int j = 1;
	u8 num;
	long r;
	char buf[512];
	GtkTextIter iter;

	r = random() % numbers_remaining;
	num = (u8)(long)ac_slist_nth_data(list, (int)r);

	numbers_remaining--;
	snprintf(buf, sizeof(buf), "%d Numbers remaining", numbers_remaining);
	gtk_text_buffer_set_text(w->sbuf, sayings[num], -1);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(w->saying), w->sbuf);
	gtk_entry_set_text(GTK_ENTRY(w->status), buf);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->random), num);

	/* Display drawn numbers */
	snprintf(buf, sizeof(buf), "%3hhu %s", num,
			(numbers_remaining % 15 == 0) ? "\n\n  " : "");
	gtk_text_buffer_insert_at_cursor(w->pbuf, buf, -1);

	/* Display a 10x9 grid of drawn numbers in numerical order */
	ordered_numbers[num - 1] = num;
	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(w->obuf), "\n", -1);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(w->obuf), &iter);
	gtk_text_buffer_insert_with_tags_by_name(w->obuf, &iter, " 01 "V_BAR
			" ", -1, "light", NULL);
	for (i = 0; i < NR_NUMBERS; i++) {
		if (ordered_numbers[i] == 0)
			snprintf(buf, sizeof(buf), "     ");
		else
			snprintf(buf, sizeof(buf), "%3hhu  ",
					ordered_numbers[i]);
		gtk_text_buffer_insert(w->obuf, &iter, buf, -1);

		if (j++ % 10 == 0 && j <= NR_NUMBERS) {
			snprintf(buf, sizeof(buf), "\n    %s\n %d1 %s ", V_BAR,
					j / 10, V_BAR);
			gtk_text_buffer_insert_with_tags_by_name(w->obuf,
					&iter, buf, -1, "light", NULL);
		}
	}

	ac_slist_remove_nth(&list, (int)r, NULL);

	if (numbers_remaining == 0)
		timer_delete(bingo_timer);
	else
		set_timer(DELAY);
}

static void init_timer(void)
{
	struct sigevent sev;
	struct sigaction action;

	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = do_bingo;
	sigaction(SIGRTMIN, &action, NULL);

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN;
	sev.sigev_value.sival_ptr = &bingo_timer;
	timer_create(CLOCK_MONOTONIC, &sev, &bingo_timer);
}

static void init(void)
{
	int i;
	struct timespec tp;

	gtk_text_buffer_set_text(w->sbuf, "Get Ready for Bingo!!!", -1);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(w->saying), w->sbuf);
	gtk_entry_set_text(GTK_ENTRY(w->status), "90 numbers remaining");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->random), 0);
	gtk_text_buffer_set_text(w->pbuf, "\n  ", -1);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(w->previous_numbers), w->pbuf);
	gtk_text_buffer_set_text(w->obuf, "", -1);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(w->ordered_numbers), w->obuf);

	memset(&ordered_numbers, 0, sizeof(ordered_numbers));
	prev_numbers[0] = '\0';
	numbers_remaining = NR_NUMBERS;

	ac_slist_destroy(&list, NULL);
	for (i = 1; i <= NR_NUMBERS; i++)
		ac_slist_add(&list, (void *)(long)i);

	clock_gettime(CLOCK_REALTIME, &tp);
	srandom(tp.tv_nsec / 2);

	init_timer();
}

void cb_play_pause(GtkButton *button __attribute__((unused)))
{
	switch (state) {
	case RUNNING:
		set_timer(0);
		state = PAUSED;
		gtk_button_set_label(GTK_BUTTON(w->start), "Play");
		break;
	case NOT_RUNNING:
		init();
		/* Fall through to do the other stuff */
		__attribute__((fallthrough));
	case PAUSED:
		set_timer(3);
		state = RUNNING;
		gtk_button_set_label(GTK_BUTTON(w->start), "Pause");
	}
}

void cb_reset(GtkButton *button __attribute__((unused)))
{
	timer_delete(bingo_timer);
	state = NOT_RUNNING;
	gtk_button_set_label(GTK_BUTTON(w->start), "Start");
	init();
}

static void get_widgets(GtkBuilder *builder)
{
	PangoFontDescription *font_desc;

	w->window = GTK_WIDGET(gtk_builder_get_object(builder, "bingo"));
	w->start = GTK_WIDGET(gtk_builder_get_object(builder, "start"));
	w->reset = GTK_WIDGET(gtk_builder_get_object(builder, "reset"));
	w->quit = GTK_WIDGET(gtk_builder_get_object(builder, "quit"));

	w->status = GTK_WIDGET(gtk_builder_get_object(builder, "status"));
	w->saying = GTK_WIDGET(gtk_builder_get_object(builder, "saying"));
	w->random = GTK_WIDGET(gtk_builder_get_object(builder, "random"));
	w->previous_numbers = GTK_WIDGET(gtk_builder_get_object(
				builder, "previous_numbers"));
	w->ordered_numbers = GTK_WIDGET(gtk_builder_get_object(
				builder, "ordered_numbers"));

	w->pbuf = GTK_TEXT_BUFFER(gtk_builder_get_object(builder, "pbuf"));
	w->obuf = GTK_TEXT_BUFFER(gtk_builder_get_object(builder, "obuf"));
	w->sbuf = GTK_TEXT_BUFFER(gtk_builder_get_object(builder, "sbuf"));

	font_desc = pango_font_description_new();

	pango_font_description_set_size(font_desc, 192 * PANGO_SCALE);
	gtk_widget_override_font(w->random, font_desc);

	pango_font_description_set_size(font_desc, 72 * PANGO_SCALE);
	gtk_widget_override_font(w->saying, font_desc);

	pango_font_description_set_size(font_desc, 24 * PANGO_SCALE);
	gtk_widget_override_font(w->status, font_desc);

	pango_font_description_set_size(font_desc, 12 * PANGO_SCALE);
	gtk_widget_override_font(w->previous_numbers, font_desc);
	gtk_widget_override_font(w->ordered_numbers, font_desc);
	pango_font_description_free(font_desc);
}


int main(int argc, char *argv[])
{
	GtkBuilder *builder;
	GError *error = NULL;

	gtk_init(&argc, &argv);

	builder = gtk_builder_new();
	if (!gtk_builder_add_from_file(builder, "bingo.glade", &error)) {
		g_warning("%s", error->message);
		exit(EXIT_FAILURE);
	}

	w = g_slice_new(struct widgets);
	get_widgets(builder);
	gtk_builder_connect_signals(builder, w);
	g_object_unref(G_OBJECT(builder));

	init();

	gtk_widget_show(w->window);
	gtk_main();

	g_slice_free(struct widgets, w);

	exit(EXIT_SUCCESS);
}
