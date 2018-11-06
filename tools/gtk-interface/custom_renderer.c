#include "custom_renderer.h"

struct _GtkCellRendererBorderPrivate
{
	gint show_border;
};

enum
{
	PROP_0,
	PROP_SHOW_BORDER,
};

G_DEFINE_TYPE( GtkCellRendererBorder, gtk_cell_renderer_border, GTK_TYPE_CELL_RENDERER_TEXT );

static void gtk_cell_renderer_border_finalize (GObject *object)
{
	G_OBJECT_CLASS (gtk_cell_renderer_border_parent_class)->finalize (object);
}

gint gtk_cell_renderer_border_get_show_border (GtkCellRendererBorder *cell)
{
	g_return_val_if_fail (GTK_IS_CELL_RENDERER_BORDER (cell), FALSE);
	return cell->priv->show_border;
}

void gtk_cell_renderer_border_set_show_border( GtkCellRendererBorder *cell, gint show_border )
{
	g_return_if_fail( GTK_IS_CELL_RENDERER_BORDER( cell ) );
	cell->priv->show_border = show_border;
}

static void gtk_cell_renderer_border_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec )
{
	switch( property_id ) {
		case PROP_SHOW_BORDER:
			g_value_set_boolean( value, gtk_cell_renderer_border_get_show_border( GTK_CELL_RENDERER_BORDER( object ) ) );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, pspec );
	}
}

static void gtk_cell_renderer_border_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *pspec )
{
	switch (property_id) {
		case PROP_SHOW_BORDER:
			gtk_cell_renderer_border_set_show_border (GTK_CELL_RENDERER_BORDER (object), g_value_get_int (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void render (
		GtkCellRenderer      *cell,
		cairo_t              *cr,
		GtkWidget            *widget,
		const GdkRectangle   *background_area,
		const GdkRectangle   *cell_area,
		GtkCellRendererState flags)
{
	GtkCellRendererBorderPrivate *priv;

	g_return_if_fail( GTK_IS_CELL_RENDERER_BORDER (cell) );

	priv = GTK_CELL_RENDERER_BORDER( cell )->priv;

	if( priv->show_border ) {
		cairo_save( cr );

		if( priv->show_border & CUSTOM_BORDER_TOP ) {
			cairo_move_to( cr, cell_area->x, cell_area->y );
			cairo_line_to( cr, cell_area->x + cell_area->width, cell_area->y );
		}
		if( priv->show_border & CUSTOM_BORDER_BOTTOM ) {
			cairo_move_to( cr, cell_area->x, cell_area->y + cell_area->height);
			cairo_line_to( cr, cell_area->x + cell_area->width, cell_area->y + cell_area->height);
		}
		cairo_close_path( cr );
		cairo_stroke( cr );
		cairo_restore( cr );
	}

	GTK_CELL_RENDERER_CLASS( gtk_cell_renderer_border_parent_class)->
		render( cell, cr, widget, background_area, cell_area, flags);
}

static void gtk_cell_renderer_border_class_init( GtkCellRendererBorderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS( klass );

	cell_class->render = render;

	object_class->finalize = gtk_cell_renderer_border_finalize;
	object_class->set_property = gtk_cell_renderer_border_set_property;
	object_class->get_property = gtk_cell_renderer_border_get_property;

	g_type_class_add_private( object_class, sizeof(GtkCellRendererBorderPrivate));

	g_object_class_install_property( object_class,
			PROP_SHOW_BORDER,
			g_param_spec_int( "show-border",
				"show-border",
				"show-border desc",
				0, 2, 0,
				G_PARAM_READWRITE));
}

static void gtk_cell_renderer_border_init( GtkCellRendererBorder *cell )
{
	cell->priv = G_TYPE_INSTANCE_GET_PRIVATE( cell,
			GTK_TYPE_CELL_RENDERER_BORDER,
			GtkCellRendererBorderPrivate);
	cell->priv->show_border = 0;

	/* we need extra padding on the side */
	/*g_object_set (cell, "xpad", 3, "ypad", 3, NULL);*/
	g_object_set (cell, "xalign", 0.5, NULL);
}

	GtkCellRenderer*
gtk_cell_renderer_border_new ()
{
	return g_object_new (GTK_TYPE_CELL_RENDERER_BORDER, NULL);
}
