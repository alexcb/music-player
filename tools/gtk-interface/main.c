#include <gtk/gtk.h>

#include <assert.h>

#include "custom_renderer.h"
#include "library_fetcher.h"
#include "playlist.h"

Library *global_library;

const char *music_endpoint = "http://music";

static void insert_all_tracks( Library *library, GtkTreeModel *src_model, GtkTreeIter *src_iter, GtkListStore *dst_store, GtkTreeIter *dst_iter, int level);

enum
{
	PLAYLIST_COL_ARTIST = 0,
	PLAYLIST_COL_ALBUM,
	PLAYLIST_COL_YEAR,
	PLAYLIST_COL_TRACK,
	PLAYLIST_COL_TRACK_NUMBER,
	PLAYLIST_COL_DURATION,
	PLAYLIST_COL_PATH,
	PLAYLIST_COL_SHOW_BORDER,
	PLAYLIST_NUM_COLS
};

enum {
	TARGET_STRING,
	TARGET_INTEGER,
	TARGET_FLOAT
};

static GtkTargetEntry *drag_target;

#define TEXT_X_PLAYLIST_ITEM "text/x-playlist-item"
#define TEXT_X_LIBRARY_ITEM "text/x-library-item"

static const GtkTargetEntry queuelike_targets[] = {
	{
		TEXT_X_PLAYLIST_ITEM,
		GTK_TARGET_SAME_WIDGET,
		0
	},
	{
		TEXT_X_LIBRARY_ITEM,
		GTK_TARGET_SAME_APP|GTK_TARGET_OTHER_WIDGET,
		1
	},
};

static const char *css =
  //".view:hover { "
  //"  color: #FF0000; "
  //"  border-top: 1px solid #000000; "
  //"}"

  ".view { "
  "  color: #FFAA00; "
  "}"
;

static void drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer user_data)
{
    //g_signal_stop_emission_by_name (widget, "drag-begin");

    //model->dragging = true;

    ///* If button_press_cb preserved a multiple selection, tell button_release_cb
    // * not to clear it. */
    //model->frozen = false;
}

static void drag_end(GtkWidget * widget, GdkDragContext * context, gpointer user_data)
{
    //g_signal_stop_emission_by_name (widget, "drag-end");

    //model->dragging = false;
    //model->clicked_row = -1;
}

static gboolean get_drop_loc(GtkWidget *widget, int x, int y, GtkTreeIter *iter )
{
	GtkTreeViewDropPosition pos;

	GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget) );

	GtkTreePath *path;
	gboolean ok = gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
			x, y,
			&path,
			&pos);
	if( ok ) {
		gint depth = gtk_tree_path_get_depth( path );
		assert( depth == 1 );

		printf("depth %d\n", depth);
		switch( pos ) {
			case GTK_TREE_VIEW_DROP_BEFORE:
				break;
			case GTK_TREE_VIEW_DROP_AFTER:
				break;
			case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
				pos = GTK_TREE_VIEW_DROP_BEFORE;
				break;
			case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
				pos = GTK_TREE_VIEW_DROP_AFTER;
				break;
			default:
				printf("Other\n");
				assert( 0 );
		}

		if( pos == GTK_TREE_VIEW_DROP_BEFORE ) {
			printf("moving back one\n");
			if( gtk_tree_path_prev( path ) ) {
				pos = GTK_TREE_VIEW_DROP_AFTER;
			} else {
				// case where item should be inserted at front of list
				if(path)
					gtk_tree_path_free(path);
				return FALSE;
			}
		}

		ok = gtk_tree_model_get_iter( model, iter, path );
		assert( ok );
	} else {
		printf("TODO determine if it should be at the bottom instead?\n");
		return FALSE;
		//printf("y = %d\n", y);
		//GtkTreeIter tmpIter;
		//ok = gtk_tree_model_get_iter_first( model, &tmpIter );
		//if( y < 20 ) {
		//	*pos = GTK_TREE_VIEW_DROP_BEFORE;
		//	*iter = tmpIter;
		//} else {
		//	while( ok ) {
		//		*iter = tmpIter;
		//		ok = gtk_tree_model_iter_next( model, &tmpIter );
		//	}
		//	*pos = GTK_TREE_VIEW_DROP_AFTER;
		//}
	}
	if(path)
		gtk_tree_path_free(path);

	return TRUE;
}

GtkTreePath *prev_hovered_path = NULL;
static void clear_prev_hovered_path(GtkWidget *widget)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget) );
	gboolean ok;
	if( prev_hovered_path ) {
		ok = gtk_tree_model_get_iter( model, &iter, prev_hovered_path );
		assert( ok );

		gtk_list_store_set( GTK_LIST_STORE(model), &iter,
				PLAYLIST_COL_SHOW_BORDER, 0,
    	        -1);

		gtk_tree_path_free( prev_hovered_path );
		prev_hovered_path = NULL;
	}
}

static gboolean drag_motion(GtkWidget *widget, GdkDragContext *context, int x, int y, unsigned time, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget) );
	gboolean ok;

	clear_prev_hovered_path( widget );

    g_signal_stop_emission_by_name( widget, "drag-motion" );

	ok = get_drop_loc( widget, x, y, &iter );
	if( ok ) {
		gtk_list_store_set( GTK_LIST_STORE(model), &iter,
			PLAYLIST_COL_SHOW_BORDER, CUSTOM_BORDER_BOTTOM,
    	    -1);
		prev_hovered_path = gtk_tree_model_get_path( model, &iter );
	} else {
		// TODO get first iterator and show the top
		//gtk_list_store_set( GTK_LIST_STORE(model), &iter,
		//		PLAYLIST_COL_SHOW_BORDER, CUSTOM_BORDER_TOP,
    	//        -1);
	}
	gdk_drag_status(context, GDK_ACTION_MOVE, time);

	if(path)
		gtk_tree_path_free(path);

	//autoscroll_add(GTK_TREE_VIEW(widget));
	return TRUE;
}

static void drag_leave(GtkWidget *widget, GdkDragContext *context, unsigned time, gpointer user_data)
{
	g_signal_stop_emission_by_name( widget, "drag-leave" );
	printf("drag-leave called\n");
	clear_prev_hovered_path( widget );
}

static gboolean drag_drop(GtkWidget *widget, GdkDragContext *context, int x, int y, unsigned time, gpointer user_data)
{
	g_signal_stop_emission_by_name( widget, "drag-data" );
	printf("got drop\n");
	return TRUE;
}

void delete_selected( GtkWidget *widget )
{
	gboolean ok;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget) );

	ok = gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) ), &model, &iter );
	if( !ok ) {
		printf("gtk_tree_view_get_selection failed\n");
		return;
	}
	gtk_list_store_remove( GTK_LIST_STORE(model), &iter );
}

gboolean on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	switch( event->keyval )
	{
		case GDK_KEY_Delete:
			printf("got delete\n" ); 
			delete_selected( widget );
			return TRUE;
	}
	return FALSE;
}

static void remove_item_visitor(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
	gtk_list_store_remove( GTK_LIST_STORE(model), iter );
}

static void drag_data_received (GtkWidget * widget, GdkDragContext *context, int x, int y, GtkSelectionData * sel, unsigned info, unsigned time, gpointer user_data)
{
	//const guchar *path_str;
	gint len;
	GtkTreeIter library_iter;
	//GtkTreeIter iter;
	GtkTreeIter insert_iter;
	GtkTreeIter *iter_ptr;
	GtkTreeViewDropPosition insertPos;
	gboolean ok = get_drop_loc( widget, x, y, &insert_iter );
	iter_ptr = ok ? &insert_iter : NULL;

	assert( insertPos = GTK_TREE_VIEW_DROP_AFTER );

	GtkTreeStore *library_tree_store = (GtkTreeStore*) user_data;

	GdkAtom target_type = gtk_selection_data_get_target( sel );
	//GdkAtom data_type = gtk_selection_data_get_data_type( sel );

	GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget) );

	if( target_type == gdk_atom_intern_static_string( TEXT_X_PLAYLIST_ITEM ) ) {
		//const guchar *path_str = gtk_selection_data_get_data_with_length( sel, &len );
		//assert( gtk_tree_model_get_iter_from_string( GTK_TREE_MODEL( library_tree_store ), &library_iter, path_str ) );

		//get_all_tracks( GTK_TREE_MODEL( library_tree_store ), &library_iter, 0 );

		//GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget) );

		//// Add a new item
		//if( insertPos == GTK_TREE_VIEW_DROP_BEFORE ) {
		//	gtk_list_store_insert_before( GTK_LIST_STORE(model), &iter, &insertIter );
		//} else {
		//	gtk_list_store_insert_after( GTK_LIST_STORE(model), &iter, &insertIter );
		//}
		//gtk_list_store_set( GTK_LIST_STORE(model), &iter,
		//		COL_NAME, "hellos",
    	//        COL_AGE, 73,
		//		COL_BACKGROUND, "#A93CD2",
		//		COL_SHOW_BORDER, 0,
    	//        -1);

		//// remove an item
		//GtkTreeSelection *selected_rows = gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) );
		//gtk_tree_selection_selected_foreach( selected_rows, remove_item_visitor, NULL );
		return;
	}

	if( target_type == gdk_atom_intern_static_string( TEXT_X_LIBRARY_ITEM ) ) {
		const gchar *path_str = (const gchar*) gtk_selection_data_get_data_with_length( sel, &len );
		assert( gtk_tree_model_get_iter_from_string( GTK_TREE_MODEL( library_tree_store ), &library_iter, path_str ) );


		printf("inserting at %p\n", iter_ptr);
		insert_all_tracks( global_library, GTK_TREE_MODEL( library_tree_store ), &library_iter, GTK_LIST_STORE(model), iter_ptr, 0 );



		//gint len;
		//const guchar *ptr = gtk_selection_data_get_data_with_length( sel, &len );
		////printf("got %x %d\n", ptr, len);
		//printf("payload =  %.*s\n", len, ptr );

		//gtk_tree_path_new_from_string( ptr );

		//char *s = malloc(len+1);
		//memcpy( s, ptr, len );
		//s[len] = '\0';

		//GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget) );

		//// Add a new item
		//GtkTreeIter iter;
		//if( insertPos == GTK_TREE_VIEW_DROP_BEFORE ) {
		//	gtk_list_store_insert_before( GTK_LIST_STORE(model), &iter, &insertIter );
		//} else {
		//	gtk_list_store_insert_after( GTK_LIST_STORE(model), &iter, &insertIter );
		//}
		//gtk_list_store_set( GTK_LIST_STORE(model), &iter,
		//		COL_NAME, s,
    	//        COL_AGE, 73,
		//		COL_BACKGROUND, "#A93CD2",
		//		COL_SHOW_BORDER, 0,
    	//        -1);

		return;
	}

}


void on_row_activated( GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data )
{
	gboolean ok;
	GtkTreeIter iter;
	Playlist playlist;
	GtkTreeModel *model;
	int i;

	model = gtk_tree_view_get_model( tree_view );

	playlist.name = g_string_new("default");
	playlist.num_items = gtk_tree_model_iter_n_children( model, NULL );
	playlist.items = malloc(sizeof(PlaylistItem) * playlist.num_items );

	i = 0;
	ok = gtk_tree_model_get_iter_first( model, &iter );
	while( ok ) {
		GValue val = G_VALUE_INIT;
		gtk_tree_model_get_value( model, &iter, PLAYLIST_COL_PATH, &val );

		char *s = (char*) g_value_get_string(&val);
		playlist.items[i].path = g_string_new(s);
		printf(" - %s\n", s);
		g_value_unset( &val );
		ok = gtk_tree_model_iter_next( model, &iter );
		i++;
	}

	printf("sending\n");
	send_playlist( music_endpoint, "default", &playlist );

	// cleanup
	for( int i = 0; i < playlist.num_items; i++ ) {
		g_string_free( playlist.items[i].path, TRUE );
	}
	free( playlist.items );
	printf("sending done\n");

	assert( gtk_tree_path_get_depth( path ) == 1 );
	gint selected_track = gtk_tree_path_get_indices( path )[0];

	play_song( music_endpoint, "default", (int) selected_track, "immediate" );
}

//static void my_visitor(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
//	printf("my_visitor\n");
//}

// get is called before data-received, and copies the data via SelectionData
static void drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *sel, unsigned info, unsigned time, gpointer user_data)
{
	//GtkTreeSelection *selected_rows = gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) );
	//gtk_tree_selection_selected_foreach( selected_rows, my_visitor, NULL );

	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean ok = gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) ), &model, &iter );
	if( ok ) {
		GValue val = G_VALUE_INIT;
		gtk_tree_model_get_value( model, &iter, 0, &val );

		char *s = (char*) g_value_get_string(&val);

		gtk_selection_data_set( sel, gdk_atom_intern_static_string("playlist_item"), 8, "hello", 5 );
		gtk_selection_data_set( sel, gdk_atom_intern_static_string("new_playlist_item"), 8, s, strlen(s) );

		g_value_unset( &val );
	}
}

static void insert_all_tracks( Library *library, GtkTreeModel *src_model, GtkTreeIter *src_iter, GtkListStore *dst_store, GtkTreeIter *dst_iter, int level)
{
	GtkTreeIter child;
	gboolean ok;
	GValue val = G_VALUE_INIT;
	GtkTreeIter new_iter;

	for(;;) {
		ok = gtk_tree_model_iter_children( src_model, &child, src_iter);
		if( ok ) {
			printf("recurse\n");
			insert_all_tracks( library, src_model, &child, dst_store, dst_iter, level+1 );
		} else {
			// leaf node which represents a track
			gtk_tree_model_get_value( src_model, src_iter, 1, &val );
			Track *track = (Track*) g_value_get_pointer(&val);
			gtk_list_store_insert_after( dst_store, &new_iter, dst_iter );
			// TODO this set should be combined with the other location where this is set, to ensure the code is kept in sync
			gtk_list_store_set( dst_store, &new_iter,
					PLAYLIST_COL_ARTIST, track->album->artist->artist->str,
					PLAYLIST_COL_ALBUM, track->album->title->str,
					PLAYLIST_COL_YEAR, track->album->year,
					PLAYLIST_COL_TRACK, track->title->str,
					PLAYLIST_COL_TRACK_NUMBER, track->number,
					PLAYLIST_COL_DURATION, track->duration,
					PLAYLIST_COL_PATH, track->path->str,
					PLAYLIST_COL_SHOW_BORDER, 0,
					-1);
			dst_iter = &new_iter;
			g_value_unset( &val );
		}

		ok = gtk_tree_model_iter_next( src_model, src_iter );
		if( !ok || level == 0 ) {
			break;
		}
	}
}

static void drag_data_get_from_library (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *sel, unsigned info, unsigned time, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean ok = gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) ), &model, &iter );
	if( ok ) {
		GtkTreePath *path = gtk_tree_model_get_path( model, &iter );
		gchar *path_s = gtk_tree_path_to_string( path );
		printf("drag_data_get_from_library sending %s\n", path_s);
		gtk_selection_data_set( sel, gdk_atom_intern_static_string("new_playlist_item"), 8, path_s, strlen(path_s)+1 );
		g_free( path_s );
		gtk_tree_path_free( path );
	} else {
		printf("NOT OK!\n");
	}
}

GtkListStore* create_playlist_store( Library *library, Playlist *playlist )
{
	int i;
	GtkTreeIter iter;
	GtkListStore *store;
	Track *track = NULL;
   
	// artist, album, track, track_num, duration (seconds), show_top_border
	store = gtk_list_store_new( PLAYLIST_NUM_COLS,
			G_TYPE_STRING, // artist
			G_TYPE_STRING, // album
			G_TYPE_INT,    // year
			G_TYPE_STRING, // track
			G_TYPE_INT,    // track_number
			G_TYPE_INT,    // duration
			G_TYPE_STRING, // path
			G_TYPE_INT     // show border
			);

	for( i = 0; i < playlist->num_items; i++ ) {
		assert( !library_lookup_by_path( library, playlist->items[i].path->str, &track ) );

		gtk_list_store_append (store, &iter);
		gtk_list_store_set( store, &iter,
				PLAYLIST_COL_ARTIST, track->album->artist->artist->str,
				PLAYLIST_COL_ALBUM, track->album->title->str,
				PLAYLIST_COL_YEAR, track->album->year,
				PLAYLIST_COL_TRACK, track->title->str,
				PLAYLIST_COL_TRACK_NUMBER, track->number,
				PLAYLIST_COL_DURATION, track->duration,
				PLAYLIST_COL_PATH, track->path->str,
				PLAYLIST_COL_SHOW_BORDER, 0,
				-1);
	}
	return store;
}

GtkTreeStore* create_library_store( Library *library )
{
	int i, j, k;
	GtkTreeStore  *store;
	GtkTreeIter artist_iter, album_iter, track_iter;

	store = gtk_tree_store_new( 2, G_TYPE_STRING, G_TYPE_POINTER );
	for( i = 0; i < library->num_artists; i++ ) {
		gtk_tree_store_append( store, &artist_iter, NULL );
		gtk_tree_store_set( store, &artist_iter,
				0, library->artists[i].artist->str,
				1, NULL,
				-1);

		for( j = 0; j < library->artists[i].num_albums; j++ ) {
			gtk_tree_store_append( store, &album_iter, &artist_iter );
			gtk_tree_store_set( store, &album_iter,
					0, library->artists[i].albums[j].title->str,
					1, NULL,
					-1);

			for( k = 0; k < library->artists[i].albums[j].num_tracks; k++ ) {
				gtk_tree_store_append( store, &track_iter, &album_iter );
				gtk_tree_store_set( store, &track_iter,
						0, library->artists[i].albums[j].tracks[k].title->str,
						1, (gpointer) &(library->artists[i].albums[j].tracks[k]),
						-1);
			}
		}
	}
	return store;
}


GtkWidget* create_library_widget( GtkTreeStore *library_tree_store )
{
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *view;

	view = gtk_tree_view_new();

	/* --- Column #1 --- */

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
			-1,
			"item",
			renderer,
			"text", 0,
			NULL);

	model = GTK_TREE_MODEL( library_tree_store );
	gtk_tree_view_set_model( GTK_TREE_VIEW( view ), model );

	gtk_drag_source_set( view,
			GDK_BUTTON1_MASK,
			&queuelike_targets[1],
			1,
			GDK_ACTION_COPY);

	g_signal_connect( view, "drag-data-get", G_CALLBACK(drag_data_get_from_library), model);
	return view;
}

GtkWidget* create_playlist_widget( GtkTreeStore *library_tree_store, GtkListStore *playlist_list_store )
{
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *view;

	view = gtk_tree_view_new();

	renderer = gtk_cell_renderer_border_new();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
			-1,
			"artist",
			renderer,
			"text", PLAYLIST_COL_ARTIST,
			"show-border", PLAYLIST_COL_SHOW_BORDER,
			NULL);

	renderer = gtk_cell_renderer_border_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
			-1,
			"album",
			renderer,
			"text", PLAYLIST_COL_ALBUM,
			"show-border", PLAYLIST_COL_SHOW_BORDER,
			NULL);

	renderer = gtk_cell_renderer_border_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
			-1,
			"year",
			renderer,
			"text", PLAYLIST_COL_YEAR,
			"show-border", PLAYLIST_COL_SHOW_BORDER,
			NULL);

	renderer = gtk_cell_renderer_border_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
			-1,
			"title",
			renderer,
			"text", PLAYLIST_COL_TRACK,
			"show-border", PLAYLIST_COL_SHOW_BORDER,
			NULL);

	renderer = gtk_cell_renderer_border_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
			-1,
			"track",
			renderer,
			"text", PLAYLIST_COL_TRACK_NUMBER,
			"show-border", PLAYLIST_COL_SHOW_BORDER,
			NULL);

	renderer = gtk_cell_renderer_border_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
			-1,
			"duration",
			renderer,
			"text", PLAYLIST_COL_DURATION,
			"show-border", PLAYLIST_COL_SHOW_BORDER,
			NULL);

	model = GTK_TREE_MODEL( playlist_list_store );
	gtk_tree_view_set_model( GTK_TREE_VIEW(view), model );

	gtk_drag_source_set( view,
			GDK_BUTTON1_MASK,
			queuelike_targets,
			2,
			GDK_ACTION_MOVE);
	gtk_drag_dest_set( view,
			GTK_DEST_DEFAULT_HIGHLIGHT|GTK_DEST_DEFAULT_DROP,
			queuelike_targets,
			2,
			GDK_ACTION_MOVE|GDK_ACTION_COPY);

	g_signal_connect( view, "drag-motion", G_CALLBACK(drag_motion), model );
	g_signal_connect( view, "drag-leave", G_CALLBACK(drag_leave), model );
	g_signal_connect( view, "drag-data-get", G_CALLBACK(drag_data_get), model );
	g_signal_connect( view, "drag-data-received", G_CALLBACK(drag_data_received), library_tree_store );
	g_signal_connect( view, "row-activated", G_CALLBACK(on_row_activated), library_tree_store );
	g_signal_connect( view, "key-press-event", G_CALLBACK(on_key_press), NULL);


	return view;
}

void dumpLibrary(Library *library) {
	for( int i = 0; i < library->num_artists; i++ ) {
		printf("%d\n", i);
		for( int j = 0; j < library->artists[i].num_albums; j++ ) {
			printf("%d.%d\n", i, j);
			for( int k = 0; k < library->artists[i].albums[j].num_tracks; k++ ) {
				printf("%d.%d.%d\n", i, j, k);
				printf("%p\n", library);
				printf("%p\n", library->artists);
				printf("%p\n", library->artists[i].artist);
				printf("%s - %s - %s\n", 
						library->artists[i].artist->str,
						library->artists[i].albums[j].title->str,
						library->artists[i].albums[j].tracks[k].title->str
						);
			}
		}
	}
}

Playlist* get_playlist(Playlists *playlists, const char *name)
{
	for( int i = 0; i < playlists->num_playlists; i++ ) {
		if( g_strcmp0( playlists->playlists[i].name->str, name ) == 0 ) {
			return &playlists->playlists[i];
		}
	}
	return NULL;
}


int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *paned;
	GtkWidget *sw, *sw2;
	GtkCssProvider *provider;

	gtk_init(&argc, &argv);

	if( argc == 2 ) {
		music_endpoint = argv[1];
	}

	Library *libraryDetails;
	int err = fetch_library( music_endpoint, &libraryDetails );
	if( err != 0 ) {
		printf("failed to fetch library\n");
		return 1;
	}
	global_library = libraryDetails;

	Playlists playlists;
	err = fetch_playlists( music_endpoint, &playlists );
	if( err != 0 ) {
		printf("failed to fetch playlists\n");
		return 1;
	}

	GtkTreeStore *library_tree_store = create_library_store( libraryDetails );
	GtkListStore *playlist_list_store = create_playlist_store( libraryDetails, get_playlist(&playlists, "default") );

	GtkWidget *library_widget = create_library_widget( library_tree_store );
	GtkWidget *playlist_widget = create_playlist_widget( library_tree_store, playlist_list_store );



	provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data( provider, css, -1, NULL );
	gtk_style_context_add_provider_for_screen( gdk_screen_get_default(), GTK_STYLE_PROVIDER( provider ), 800 );

	window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title( GTK_WINDOW(window), "Music playlist" );
	gtk_window_set_default_size( GTK_WINDOW(window), 1400, 800 );

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
			GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);

	sw2 = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw2),
			GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw2),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);

	drag_target = gtk_target_entry_new("GTK_LIST_BOX_ROW", 0, 0);

	paned = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_set_position( GTK_PANED(paned), 300 );

	gtk_paned_pack1( GTK_PANED(paned), sw, TRUE, FALSE );
	gtk_widget_set_size_request( library_widget, 50, -1 );

	gtk_paned_pack2( GTK_PANED(paned), sw2, TRUE, FALSE );
	gtk_widget_set_size_request( playlist_widget, 50, -1);

	gtk_container_add( GTK_CONTAINER( sw ), library_widget );
	gtk_container_add( GTK_CONTAINER( sw2 ), playlist_widget );

	gtk_container_add( GTK_CONTAINER(window), paned );

	gtk_widget_show_all (window);

	gtk_main();
	return 0;
}
