#ifndef __GTK_CELL_RENDERER_BORDER_H__
#define __GTK_CELL_RENDERER_BORDER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#define CUSTOM_BORDER_TOP 0x01
#define CUSTOM_BORDER_BOTTOM 0x02

G_BEGIN_DECLS

#define GTK_TYPE_CELL_RENDERER_BORDER (gtk_cell_renderer_border_get_type ())

#define GTK_CELL_RENDERER_BORDER(obj)             \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),             \
  GTK_TYPE_CELL_RENDERER_BORDER,                  \
  GtkCellRendererBorder))

#define GTK_CELL_RENDERER_BORDER_CONST(obj)       \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),             \
  GTK_TYPE_CELL_RENDERER_BORDER,                  \
  GtkCellRendererBorder const))

#define GTK_CELL_RENDERER_BORDER_CLASS(klass)     \
  (G_TYPE_CHECK_CLASS_CAST ((klass),              \
  GTK_TYPE_CELL_RENDERER_BORDER,                  \
  GtkCellRendererBorderClass))

#define GTK_IS_CELL_RENDERER_BORDER(obj)          \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),             \
  GTK_TYPE_CELL_RENDERER_BORDER))

#define GTK_IS_CELL_RENDERER_BORDER_CLASS(klass)  \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),              \
  GTK_TYPE_CELL_RENDERER_BORDER))

#define GTK_CELL_RENDERER_BORDER_GET_CLASS(obj)   \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),              \
  GTK_TYPE_CELL_RENDERER_BORDER,                  \
  GtkCellRendererBorderClass))

typedef struct _GtkCellRendererBorder         GtkCellRendererBorder;
typedef struct _GtkCellRendererBorderClass    GtkCellRendererBorderClass;
typedef struct _GtkCellRendererBorderPrivate  GtkCellRendererBorderPrivate;

struct _GtkCellRendererBorder
{
	GtkCellRendererText parent;

	GtkCellRendererBorderPrivate *priv;
};

struct _GtkCellRendererBorderClass
{
	GtkCellRendererTextClass parent_class;
};

GType            gtk_cell_renderer_border_get_type        (void) G_GNUC_CONST;
GtkCellRenderer* gtk_cell_renderer_border_new             (void);
gboolean         gtk_cell_renderer_border_get_show_top_border (GtkCellRendererBorder *cell);
void             gtk_cell_renderer_border_set_show_top_border (GtkCellRendererBorder *cell, gboolean show_top_border);
gboolean         gtk_cell_renderer_border_get_show_bottom_border (GtkCellRendererBorder *cell);
void             gtk_cell_renderer_border_set_show_bottom_border (GtkCellRendererBorder *cell, gboolean show_bottom_border);

G_END_DECLS

#endif /* __GTK_CELL_RENDERER_BORDER_H__ */
