/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 CodeFactory AB
 * Copyright (C) 2001-2002 Richard Hult <richard@imendio.com>
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtktreemodel.h>
#include <libplanner/mrp-time.h>
#include "planner-marshal.h"
#include "planner-gantt-header.h"
#include "planner-gantt-model.h"
#include "planner-scale-utils.h"


struct _PlannerChartHeaderPriv {
	GdkWindow     *bin_window;
	
	GtkAdjustment *hadjustment;

	PangoLayout   *layout;

	PlannerScaleUnit    major_unit;
	PlannerScaleFormat  major_format;

	PlannerScaleUnit    minor_unit;
	PlannerScaleFormat  minor_format;
	
	gdouble        hscale;
	
	gint           width;
	gint           height;

	gdouble        x1;
	gdouble        x2;
};

/* Properties */
enum {
	PROP_0,
	PROP_HEIGHT,
	PROP_X1,
	PROP_X2,
	PROP_SCALE,
	PROP_ZOOM,
};

static void     chart_header_class_init           (PlannerChartHeaderClass *klass);
static void     chart_header_init                 (PlannerChartHeader      *header);
static void     chart_header_finalize             (GObject            *object);
static void     chart_header_set_property         (GObject            *object,
						   guint               prop_id,
						   const GValue       *value,
						   GParamSpec         *pspec);
static void     chart_header_get_property         (GObject            *object,
						   guint               prop_id,
						   GValue             *value,
						   GParamSpec         *pspec);
static void     chart_header_destroy              (GtkObject          *object);
static void     chart_header_map                  (GtkWidget          *widget);
static void     chart_header_realize              (GtkWidget          *widget);
static void     chart_header_unrealize            (GtkWidget          *widget);
static void     chart_header_size_allocate        (GtkWidget          *widget,
						   GtkAllocation      *allocation);
static gboolean chart_header_expose_event         (GtkWidget          *widget,
						   GdkEventExpose     *event);
static void     chart_header_set_adjustments      (PlannerChartHeader      *header,
						   GtkAdjustment      *hadj,
						   GtkAdjustment      *vadj);
static void     chart_header_adjustment_changed   (GtkAdjustment      *adjustment,
						   PlannerChartHeader      *header);


static GtkWidgetClass *parent_class = NULL;


GtkType
planner_chart_header_get_type (void)
{
	static GtkType planner_chart_header_type = 0;

	if (!planner_chart_header_type) {
		static const GTypeInfo planner_chart_header_info = {
			sizeof (PlannerChartHeaderClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) chart_header_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (PlannerChartHeader),
			0,              /* n_preallocs */
			(GInstanceInitFunc) chart_header_init
		};

		planner_chart_header_type = g_type_register_static (
			GTK_TYPE_WIDGET,
			"PlannerChartHeader",
			&planner_chart_header_info,
			0);
	}
	
	return planner_chart_header_type;
}

static void
chart_header_class_init (PlannerChartHeaderClass *class)
{
	GObjectClass      *o_class;
	GtkObjectClass    *object_class;
	GtkWidgetClass    *widget_class;
	GtkContainerClass *container_class;

	parent_class = g_type_class_peek_parent (class);

	o_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	container_class = (GtkContainerClass *) class;

	/* GObject methods. */
	o_class->set_property = chart_header_set_property;
	o_class->get_property = chart_header_get_property;
	o_class->finalize = chart_header_finalize;

	/* GtkObject methods. */
	object_class->destroy = chart_header_destroy;

	/* GtkWidget methods. */
	widget_class->map = chart_header_map;;
	widget_class->realize = chart_header_realize;
	widget_class->unrealize = chart_header_unrealize;
	widget_class->size_allocate = chart_header_size_allocate;
	widget_class->expose_event = chart_header_expose_event;

	class->set_scroll_adjustments = chart_header_set_adjustments;
		
	widget_class->set_scroll_adjustments_signal =
		g_signal_new ("set_scroll_adjustments",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PlannerChartHeaderClass, set_scroll_adjustments),
			      NULL, NULL,
			      planner_marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE, 2,
			      GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
	
	/* Properties. */
	g_object_class_install_property (
		o_class,
		PROP_HEIGHT,
		g_param_spec_int ("height",
				  NULL,
				  NULL,
				  0, G_MAXINT, 0,
				  G_PARAM_READWRITE));

	g_object_class_install_property (
		o_class,
		PROP_X1,
		g_param_spec_double ("x1",
				     NULL,
				     NULL,
				     -1, G_MAXDOUBLE, -1,
				     G_PARAM_READWRITE));

	g_object_class_install_property (
		o_class,
		PROP_X2,
		g_param_spec_double ("x2",
				     NULL,
				     NULL,
				     -1, G_MAXDOUBLE, -1,
				     G_PARAM_READWRITE));

	g_object_class_install_property (
		o_class,
		PROP_SCALE,
		g_param_spec_double ("scale",
				     NULL,
				     NULL,
				     0.000001, G_MAXDOUBLE, 1.0,
				     G_PARAM_WRITABLE));

	g_object_class_install_property (
		o_class,
		PROP_ZOOM,
		g_param_spec_double ("zoom",
				     NULL,
				     NULL,
				     -G_MAXDOUBLE, G_MAXDOUBLE, 7,
				     G_PARAM_WRITABLE));
}

static void
chart_header_init (PlannerChartHeader *header)
{
	PlannerChartHeaderPriv *priv;

	gtk_widget_set_redraw_on_allocate (GTK_WIDGET (header), FALSE);

	priv = g_new0 (PlannerChartHeaderPriv, 1);
	header->priv = priv;

	chart_header_set_adjustments (header, NULL, NULL);

	priv->hscale = 1.0;
	priv->x1 = 0;
	priv->x2 = 0;
	priv->height = -1;
	priv->width = -1;

	priv->major_unit = PLANNER_SCALE_UNIT_MONTH;
	priv->minor_unit = PLANNER_SCALE_UNIT_WEEK;

	priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (header),
						       NULL);
}

static void
chart_header_set_zoom (PlannerChartHeader *header, gdouble zoom)
{
	PlannerChartHeaderPriv *priv;
	gint               level;

	priv = header->priv;

	level = planner_scale_clamp_zoom (zoom);
	
	priv->major_unit = planner_scale_conf[level].major_unit;
	priv->major_format = planner_scale_conf[level].major_format;

	priv->minor_unit = planner_scale_conf[level].minor_unit;
	priv->minor_format = planner_scale_conf[level].minor_format;
}

static void
chart_header_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	PlannerChartHeader     *header;
	PlannerChartHeaderPriv *priv;
	gdouble            tmp;
	gint               width;
	gdouble            tmp_scale;
	gboolean           change_width = FALSE;
	gboolean           change_height = FALSE;
	gboolean           change_scale = FALSE;

	header = PLANNER_CHART_HEADER (object);
	priv = header->priv;

	switch (prop_id) {
	case PROP_HEIGHT:
		priv->height = g_value_get_int (value);
		change_height = TRUE;
		break;
	case PROP_X1:
		tmp = g_value_get_double (value);
		if (tmp != priv->x1) {
			priv->x1 = tmp;
			change_width = TRUE;
		}
		break;
	case PROP_X2:
		tmp = g_value_get_double (value);
		if (tmp != priv->x2) {
			priv->x2 = tmp;
			change_width = TRUE;
		}
		break;
	case PROP_SCALE:
		tmp_scale = g_value_get_double (value);
		if (tmp_scale != priv->hscale) {
			priv->hscale = tmp_scale;
			change_scale = TRUE;
		}
		break;
	case PROP_ZOOM:
		chart_header_set_zoom (header, g_value_get_double (value));
		break;
	default:
		break;
	}

	if (change_width) {
		if (priv->x1 > 0 && priv->x2 > 0) {
			width = floor (priv->x2 - priv->x1 + 0.5);

			/* If both widths aren't set yet, this can happen: */
			if (width < -1) {
				width = -1;
			}
		} else {
			width = -1;
		}
		priv->width = width;
	}

	if (change_width || change_height) {
		gtk_widget_set_size_request (GTK_WIDGET (header),
					     priv->width, 
					     priv->height);
	}

	if ((change_width || change_height || change_scale) && GTK_WIDGET_REALIZED (header)) {
		gdk_window_invalidate_rect (priv->bin_window,
					    NULL,
					    FALSE);
	}
}

static void
chart_header_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	PlannerChartHeader *header;

	header = PLANNER_CHART_HEADER (object);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
chart_header_finalize (GObject *object)
{
	PlannerChartHeader *header = PLANNER_CHART_HEADER (object);

	g_free (header->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
chart_header_destroy (GtkObject *object)
{
	/*PlannerChartHeader *header = PLANNER_CHART_HEADER (object);*/

	/* FIXME: free stuff. */

	if (GTK_OBJECT_CLASS (parent_class)->destroy) {
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
	}
}

static void
chart_header_map (GtkWidget *widget)
{
	PlannerChartHeader *header;

	g_return_if_fail (PLANNER_IS_CHART_HEADER (widget));
	
	header = PLANNER_CHART_HEADER (widget);
	
	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

	gdk_window_show (header->priv->bin_window);

	gdk_window_show (widget->window);
}

static void
chart_header_realize (GtkWidget *widget)
{
	PlannerChartHeader *header;
	GdkWindowAttr  attributes;
	GdkGCValues    values;
	gint           attributes_mask;
  
	g_return_if_fail (PLANNER_IS_CHART_HEADER (widget));
	
	header = PLANNER_CHART_HEADER (widget);
	
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	/* Create the main, clipping window. */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	/* Bin window. */
	attributes.x = 0;
	attributes.y = header->priv->height;
	attributes.width = header->priv->width;
	attributes.height = widget->allocation.height;
	attributes.event_mask = GDK_EXPOSURE_MASK |
		GDK_SCROLL_MASK |
		GDK_POINTER_MOTION_MASK |
		GDK_ENTER_NOTIFY_MASK |
		GDK_LEAVE_NOTIFY_MASK |
		GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK |
		gtk_widget_get_events (widget);

	header->priv->bin_window = gdk_window_new (widget->window,
						   &attributes,
						   attributes_mask);
	gdk_window_set_user_data (header->priv->bin_window, widget);
	
	values.foreground = (widget->style->white.pixel == 0 ?
			     widget->style->black : widget->style->white);
	values.function = GDK_XOR;
	values.subwindow_mode = GDK_INCLUDE_INFERIORS;
	
	widget->style = gtk_style_attach (widget->style, widget->window);
	gdk_window_set_background (widget->window,
				   &widget->style->base[widget->state]);
	gdk_window_set_background (header->priv->bin_window,
				   &widget->style->base[widget->state]);
}

static void
chart_header_unrealize (GtkWidget *widget)
{
	PlannerChartHeader *header;

	g_return_if_fail (PLANNER_IS_CHART_HEADER (widget));

	header = PLANNER_CHART_HEADER (widget);

	gdk_window_set_user_data (header->priv->bin_window, NULL);
	gdk_window_destroy (header->priv->bin_window);
	header->priv->bin_window = NULL;

	if (GTK_WIDGET_CLASS (parent_class)->unrealize) {
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
	}
}

static void
chart_header_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
	PlannerChartHeader *header;

	g_return_if_fail (PLANNER_IS_CHART_HEADER (widget));

	header = PLANNER_CHART_HEADER (widget);

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (widget->window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);
		gdk_window_move_resize (header->priv->bin_window,
					- (gint) header->priv->hadjustment->value,
					0,
					MAX (header->priv->width, allocation->width),
					allocation->height);
	}
}

static gboolean
chart_header_expose_event (GtkWidget      *widget,
			   GdkEventExpose *event)
{
	PlannerChartHeader     *header;
	PlannerChartHeaderPriv *priv;
	gint               width, height;
	gdouble            hscale;
	gint               x;
	mrptime            t0;
	mrptime            t1;
	mrptime            t;
	gchar             *str;
	gint               minor_width;
	gint               major_width;
	GdkGC             *gc;
	GdkRectangle       rect;
	
	header = PLANNER_CHART_HEADER (widget);
	priv = header->priv;
	hscale = priv->hscale;

	t0 = floor ((priv->x1 + event->area.x) / hscale + 0.5);
	t1 = floor ((priv->x1 + event->area.x + event->area.width) / hscale + 0.5);

	gdk_drawable_get_size (event->window, &width, &height);

	/* Draw background. We only draw over the exposed area, padding with +/-
	 * 5 so we don't mess up the header with button edges all over.
	 */
	gtk_paint_box (widget->style,
		       event->window,
		       GTK_STATE_NORMAL,
		       GTK_SHADOW_OUT,
		       &event->area,
		       widget,
		       "button",
		       event->area.x - 5,
		       0,
		       event->area.width + 10,
		       height);
	
	gdk_draw_line (event->window,
		       widget->style->fg_gc[GTK_STATE_INSENSITIVE],
		       event->area.x,
		       height / 2,
		       event->area.x + event->area.width,
		       height / 2);

	/* Get the widths of major/minor ticks so that we know how wide to make
	 * the clip region.
	 */
	major_width = hscale * (planner_scale_time_next (t0, priv->major_unit) -
				planner_scale_time_prev (t0, priv->major_unit));

	minor_width = hscale * (planner_scale_time_next (t0, priv->minor_unit) -
				planner_scale_time_prev (t0, priv->minor_unit));

	gc = gdk_gc_new (widget->window);
	gdk_gc_copy (gc, widget->style->text_gc[GTK_STATE_NORMAL]);

	rect.y = 0;
	rect.height = height;

	/* Draw the major scale. */
	if (major_width < 2 || priv->major_unit == PLANNER_SCALE_UNIT_NONE) {
		/* Unless it's too thin to make sense. */
		goto minor_ticks;
	}
	
	t = planner_scale_time_prev (t0, priv->major_unit);
	
	while (t <= t1) {
		x = floor (t * hscale - priv->x1 + 0.5);
		
		gdk_draw_line (event->window,
			       widget->style->fg_gc[GTK_STATE_INSENSITIVE],
			       x, 0,
			       x, height / 2);

		str = planner_scale_format_time (t,
					    priv->major_unit,
					    priv->major_format);
		pango_layout_set_text (priv->layout,
				       str,
				       -1);
		g_free (str);

		rect.x = x;
		rect.width = major_width;
		gdk_gc_set_clip_rectangle (gc, &rect);

		gdk_draw_layout (event->window,
				 gc,
				 x + 3,
				 2,
				 priv->layout);
		
		t = planner_scale_time_next (t, priv->major_unit);
	}

 minor_ticks:

	/* Draw the minor scale. */
	if (minor_width < 2 || priv->major_unit == PLANNER_SCALE_UNIT_NONE) {
		/* Unless it's too thin to make sense. */
		goto done;
	}
	
	t = planner_scale_time_prev (t0, priv->minor_unit);

	while (t <= t1) {
		x = floor (t * hscale - priv->x1 + 0.5);

		gdk_draw_line (event->window,
			       widget->style->fg_gc[GTK_STATE_INSENSITIVE],
			       x, height / 2,
			       x, height);

		str = planner_scale_format_time (t,
					    priv->minor_unit,
					    priv->minor_format);
		pango_layout_set_text (priv->layout,
				       str,
				       -1);
		g_free (str);
		
		rect.x = x;
		rect.width = minor_width;
		gdk_gc_set_clip_rectangle (gc, &rect);

		gdk_draw_layout (event->window,
				 gc,
				 x + 3,
				 height / 2 + 2,
				 priv->layout);

		t = planner_scale_time_next (t, priv->minor_unit);
	}

 done:
	gdk_gc_unref (gc);
	
	return TRUE;
}

/* Callbacks */
static void
chart_header_set_adjustments (PlannerChartHeader *header,
			      GtkAdjustment *hadj,
			      GtkAdjustment *vadj)
{
	g_return_if_fail (hadj == NULL || GTK_IS_ADJUSTMENT (hadj));
	g_return_if_fail (vadj == NULL || GTK_IS_ADJUSTMENT (vadj));

	if (hadj == NULL) {
		hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	}
	
	if (header->priv->hadjustment && (header->priv->hadjustment != hadj)) {
		gtk_object_unref (GTK_OBJECT (header->priv->hadjustment));
	}
	
	if (header->priv->hadjustment != hadj) {
		header->priv->hadjustment = hadj;
		gtk_object_ref (GTK_OBJECT (header->priv->hadjustment));
		gtk_object_sink (GTK_OBJECT (header->priv->hadjustment));

		g_signal_connect (hadj,
				  "value_changed",
				  G_CALLBACK (chart_header_adjustment_changed),
				  header);
		
		gtk_widget_set_scroll_adjustments (GTK_WIDGET (header),
						   hadj,
						   NULL);
	}
}

static void
chart_header_adjustment_changed (GtkAdjustment *adjustment,
				 PlannerChartHeader *header)
{
	if (GTK_WIDGET_REALIZED (header)) {
		gdk_window_move (header->priv->bin_window,
				 - header->priv->hadjustment->value,
				 0);
	}
}

GtkWidget *
planner_chart_header_new (void)
{

	return g_object_new (PLANNER_TYPE_CHART_HEADER, NULL);
}
