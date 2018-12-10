#include <gtk/gtk.h>

#include <assert.h>
#include <stdbool.h>

#include "custom_renderer.h"
#include "library_fetcher.h"
#include "playlist.h"

Library *global_library = NULL;

int global_selected_playlist_index = 0;
Playlists global_playlists;

GtkWidget *global_library_widget;
GtkWidget *global_playlist_widget;
GtkWidget *global_playlist_selector_widget;
GtkWidget *global_host_selector_widget;


GtkListStore *global_host_list_store;
GtkListStore *global_playlists_list_store;
GtkTreeStore *global_library_tree_store;

GString *music_endpoint = NULL;

static void insert_all_tracks( Library *library, GtkTreeModel *src_model, GtkTreeIter *src_iter, GtkListStore *dst_store, GtkTreeIter *dst_iter, int level);
GtkListStore* create_playlist_store( Library *library, Playlist *playlist );

void set_active_playlist( int i );
bool set_active_playlist_by_name( const char *name );
void refresh( void );
void save_current_playlist( void );
void host_selector_changed( void );

// the playlist
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

// the list of playlists
enum
{
	PLAYLISTS_COL_PLAYLIST = 0,
	PLAYLISTS_COL_INDEX,
	PLAYLISTS_NUM_COLS
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
  //"}"

  ".view:focus { "
  "  color: #00A900; "
  "}"

  ".view:selected { "
  "  background-color: #E0E0E0; "
  "}"

  ".view { "
  "  color: #000000; "
  "}"
;

void on_new_playlist_name_activate( GtkWidget *dialog, GtkEntry *entry )
{
	GtkTreeIter iter;

	const gchar *playlist_name = gtk_entry_get_text( entry );
	printf("got it: %s\n", playlist_name);

	int i = add_playlist( playlist_name, &global_playlists );
	if( i >= 0 ) {
		global_playlists.playlists[i].list_store = create_playlist_store( global_library, &global_playlists.playlists[i] );

		gtk_list_store_append( global_playlists_list_store, &iter );
		gtk_list_store_set( global_playlists_list_store, &iter,
				PLAYLISTS_COL_PLAYLIST, global_playlists.playlists[i].name->str,
				PLAYLISTS_COL_INDEX, i,
				-1);
	}
	set_active_playlist_by_name( playlist_name );

	gtk_widget_destroy( dialog );
}

void quick_message( GtkWindow *parent )
{
	GtkWidget *dialog, *label, *content_area, *entry;
	GtkDialogFlags flags;

	dialog = gtk_dialog_new();
	gtk_window_set_title( GTK_WINDOW(dialog), "New playlist" );

	entry = gtk_entry_new();

	content_area = gtk_dialog_get_content_area( GTK_DIALOG( dialog ) );
	label = gtk_label_new( "Enter New playlist name" );

	// Ensure that the dialog box is destroyed when the user responds
	g_signal_connect_swapped( dialog,
			"response",
			G_CALLBACK( gtk_widget_destroy ),
			dialog );

	g_signal_connect_swapped( entry,
			"activate",
			G_CALLBACK( on_new_playlist_name_activate ),
			dialog );

	gtk_container_add( GTK_CONTAINER( content_area ), label );
	gtk_container_add( GTK_CONTAINER( content_area ), entry );
	gtk_widget_show_all( dialog );
}

void accelerator_new_playlist( gpointer user_data )
{
	quick_message( NULL );
}

void accelerator_pause( gpointer user_data )
{
	music_pause( music_endpoint->str );
}

void accelerator_save( gpointer user_data )
{
	save_current_playlist();
}

void accelerator_refresh( gpointer user_data )
{
	refresh();
}

static void destroy( GtkWidget *widget, gpointer   data )
{
	gtk_main_quit();
}

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


gboolean playlist_selector_on_key_press( GtkWidget *widget, GdkEventKey *event, gpointer user_data )
{
	switch( event->keyval )
	{
		case GDK_KEY_Left:
			printf("got left\n" ); 
			printf("from playlist selector to playlist\n");
			gtk_widget_grab_focus( global_playlist_widget );
			return TRUE;
	}
	return FALSE;
}


gboolean playlist_on_key_press( GtkWidget *widget, GdkEventKey *event, gpointer user_data )
{
	switch( event->keyval )
	{
		case 65288: // delete on a mac
		case GDK_KEY_space:
		case GDK_KEY_Delete:
			printf("got delete\n" ); 
			delete_selected( widget );
			return TRUE;
		case GDK_KEY_Left:
			printf("from playlist to library\n");
			gtk_widget_grab_focus( global_library_widget );
			return TRUE;
		case GDK_KEY_Right:
			printf("from playlist to playlist selector\n");
			gtk_widget_grab_focus( global_playlist_selector_widget );
			return TRUE;
	}
	return FALSE;
}

gboolean library_on_key_press( GtkWidget *widget, GdkEventKey *event, gpointer user_data )
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;

	switch( event->keyval )
	{
		case GDK_KEY_Return:
			if( gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) ), &model, &iter ) ) {
				path = gtk_tree_model_get_path( model, &iter );
				if( gtk_tree_view_row_expanded( GTK_TREE_VIEW(widget), path ) ) {
					gtk_tree_view_collapse_row( GTK_TREE_VIEW(widget), path );
				} else {
					gtk_tree_view_expand_row( GTK_TREE_VIEW(widget), path, FALSE );
				}
				gtk_tree_path_free( path );
			}
			return TRUE;
		case GDK_KEY_space:
			if( gtk_tree_selection_get_selected( gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) ), &model, &iter ) ) {
				insert_all_tracks( global_library, model, &iter, global_playlists.playlists[global_selected_playlist_index].list_store, NULL, 0 );
			}
			return TRUE;
		case GDK_KEY_Right:
			printf("from library to playlist\n" ); 
			gtk_widget_grab_focus( global_playlist_widget );
			return TRUE;
	}
	return FALSE;
}

void library_on_row_activated( GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data )
{
	GtkTreeIter src_iter;
	GtkTreeModel *model = gtk_tree_view_get_model( tree_view );
	assert( gtk_tree_model_get_iter( model, &src_iter, path ) );
	
	insert_all_tracks( global_library, model, &src_iter, global_playlists.playlists[global_selected_playlist_index].list_store, NULL, 0 );
}

static void remove_item_visitor(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
	gtk_list_store_remove( GTK_LIST_STORE(model), iter );
}

static void drag_data_received( GtkWidget *widget, GdkDragContext *context, int x, int y, GtkSelectionData * sel, unsigned info, unsigned time, gpointer user_data)
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


void on_playlist_selector_row_activated( GtkTreeSelection *tree_selection, gpointer user_data )
{
	int playlist_index;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if( gtk_tree_selection_get_selected( tree_selection, &model, &iter ) ) {
		GValue val = G_VALUE_INIT;
		gtk_tree_model_get_value( model, &iter, PLAYLISTS_COL_INDEX, &val );
		playlist_index = (int) g_value_get_int( &val );
		g_value_unset( &val );

		printf("switched to %d\n", playlist_index);
		set_active_playlist( playlist_index );
		printf("done\n");
	} else {
		printf("failed to get selected playlist\n" );
	}
}

void save_current_playlist()
{
	gboolean ok;
	GtkTreeIter iter;
	Playlist playlist;
	GtkTreeModel *model;
	int i;

	model = gtk_tree_view_get_model( GTK_TREE_VIEW( global_playlist_widget ) );

	playlist.name = g_string_new( global_playlists.playlists[global_selected_playlist_index].name->str );
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
	send_playlist( music_endpoint->str, playlist.name->str, &playlist );

	// cleanup
	for( int i = 0; i < playlist.num_items; i++ ) {
		g_string_free( playlist.items[i].path, TRUE );
	}
	free( playlist.items );
	g_string_free( playlist.name, TRUE );
	printf("sending done\n");
}

void on_row_activated( GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data )
{
	save_current_playlist();

	assert( gtk_tree_path_get_depth( path ) == 1 );
	gint selected_track = gtk_tree_path_get_indices( path )[0];

	const char *current_playlist_name = global_playlists.playlists[global_selected_playlist_index].name->str;
	play_song( music_endpoint->str, current_playlist_name, (int) selected_track, "immediate" );
}

//static void my_visitor(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
//	printf("my_visitor\n");
//}

void set_active_playlist( int i )
{
	global_selected_playlist_index = i;
	if( global_selected_playlist_index >= global_playlists.num_playlists ) {
		printf("index overflow\n");
		global_selected_playlist_index = 0;
	}

	gtk_tree_view_set_model(
			GTK_TREE_VIEW( global_playlist_widget ),
			GTK_TREE_MODEL( global_playlists.playlists[global_selected_playlist_index].list_store )
			);
}

bool set_active_playlist_by_name( const char *name )
{
	for( int i = 0; i < global_playlists.num_playlists; i++ ) {
		if( strcmp( global_playlists.playlists[i].name->str, name ) == 0 ) {
			set_active_playlist( i );
			return true;
		}
	}
	return false;
}

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

GtkListStore* create_hosts_store( void )
{
	return gtk_list_store_new( 1,
			G_TYPE_STRING // hostname
			);
}

GtkListStore* create_playlists_store( void )
{
	// artist, album, track, track_num, duration (seconds), show_top_border
	return gtk_list_store_new( PLAYLISTS_NUM_COLS,
			G_TYPE_STRING, // name of playlist
			G_TYPE_INT // name of playlist
			);

}

void update_playlists_store( void )
{
	int i;
	GtkTreeIter iter;
	GtkListStore *store = global_playlists_list_store;
	Track *track = NULL;
	Playlists *playlists = &global_playlists;

	gtk_list_store_clear( store );
   
	bool default_found = false;
	for( i = 0; i < playlists->num_playlists; i++ ) {
		printf("add %s\n", playlists->playlists[i].name->str);
		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store, &iter,
				PLAYLISTS_COL_PLAYLIST, playlists->playlists[i].name->str,
				PLAYLISTS_COL_INDEX, i,
				-1);

		if( strcmp( playlists->playlists[i].name->str, "default" ) == 0 ) {
			default_found = true;
		}
	}

	return;
}

GtkListStore* create_playlist_store( Library *library, Playlist *playlist )
{
	int err;
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
		err = library_lookup_by_path( library, playlist->items[i].path->str, &track );
		if( err ) {
			printf("failed to lookup %s\n", playlist->items[i].path->str);
			gtk_list_store_append (store, &iter);
			gtk_list_store_set( store, &iter,
					PLAYLIST_COL_ARTIST, playlist->items[i].path->str,
					PLAYLIST_COL_PATH, playlist->items[i].path->str,
					PLAYLIST_COL_SHOW_BORDER, 0,
					-1);
		} else {
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
	}
	return store;
}

GtkTreeStore* create_library_store( void )
{
	return gtk_tree_store_new( 2, G_TYPE_STRING, G_TYPE_POINTER );
}

void update_library_store( Library *library, GtkTreeStore *store )
{
	int i, j, k;
	GtkTreeIter artist_iter, album_iter, track_iter;

	char buf[1024];

	gtk_tree_store_clear( store );

	for( i = 0; i < library->num_artists; i++ ) {
		gtk_tree_store_append( store, &artist_iter, NULL );
		gtk_tree_store_set( store, &artist_iter,
				0, library->artists[i].artist->str,
				1, NULL,
				-1);

		for( j = 0; j < library->artists[i].num_albums; j++ ) {
			sprintf( buf, "%s (%d)", library->artists[i].albums[j].title->str, library->artists[i].albums[j].year );
			gtk_tree_store_append( store, &album_iter, &artist_iter );
			gtk_tree_store_set( store, &album_iter,
					0, buf,
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
	g_signal_connect( view, "key-press-event", G_CALLBACK(library_on_key_press), NULL);
	//g_signal_connect( view, "row-activated", G_CALLBACK(library_on_row_activated), NULL );
	return view;
}

GtkWidget* create_playlist_selector_widget( GtkListStore *playlists_list_store )
{
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *view;

	view = gtk_tree_view_new();

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
			-1,
			"Playlist",
			renderer,
			"text", 0,
			NULL);

	gtk_tree_view_set_model( GTK_TREE_VIEW( view ), GTK_TREE_MODEL( playlists_list_store ) );

	g_signal_connect( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed", G_CALLBACK(on_playlist_selector_row_activated), NULL );
	g_signal_connect( view, "key-press-event", G_CALLBACK(playlist_selector_on_key_press), NULL);

	return view;
}

GtkWidget* create_playlist_widget( GtkTreeStore *library_tree_store )
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

	//model = GTK_TREE_MODEL( playlist_list_store );
	//gtk_tree_view_set_model( GTK_TREE_VIEW(view), model );

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

	g_signal_connect( view, "drag-motion", G_CALLBACK(drag_motion), NULL );
	g_signal_connect( view, "drag-leave", G_CALLBACK(drag_leave), NULL );
	g_signal_connect( view, "drag-data-get", G_CALLBACK(drag_data_get), NULL );
	g_signal_connect( view, "drag-data-received", G_CALLBACK(drag_data_received), library_tree_store );
	g_signal_connect( view, "row-activated", G_CALLBACK(on_row_activated), library_tree_store );
	g_signal_connect( view, "key-press-event", G_CALLBACK(playlist_on_key_press), NULL);


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

void refresh( void )
{
	printf("refreshing\n");

	// update library
	Library *libraryDetails;
	int err = fetch_library( music_endpoint->str, &libraryDetails );
	if( err != 0 ) {
		printf("failed to fetch library\n");
		return;
	}
	if( global_library ) free( global_library );
	global_library = libraryDetails;

	// update library tree store
	update_library_store( global_library, global_library_tree_store );


	// update playlists
	// TODO memory leak -- need to free prev playlists and gtk models
	err = fetch_playlists( music_endpoint->str, &global_playlists );
	if( err != 0 ) {
		printf("failed to fetch playlists\n");
		return;
	}
	for( int i = 0; i < global_playlists.num_playlists; i++ ) {
		global_playlists.playlists[i].list_store = create_playlist_store( libraryDetails, &global_playlists.playlists[i] );
	}
	assert( global_playlists.num_playlists > 0 );
	update_playlists_store();

	assert( set_active_playlist_by_name( "default" ) );
}

void populate_hosts( GtkListStore *store, const char **argv )
{
	GtkTreeIter iter;
	for( ; *argv != NULL; argv++ ) {
		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store, &iter,
			0, *argv,
			-1);
	}
}

void host_selector_changed( void )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_combo_box_get_model( GTK_COMBO_BOX(global_host_selector_widget) );
	GValue val = G_VALUE_INIT;
	const char *s;

	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX(global_host_selector_widget), &iter ) ) {
		gtk_tree_model_get_value( model, &iter, 0, &val );

		s = (const char*) g_value_get_string( &val );
		if( music_endpoint != NULL ) {
			g_string_free( music_endpoint, TRUE );
		}
		music_endpoint = g_string_new(s);
		printf("changed host to %s\n", s);
		g_value_unset( &val );

	}
	refresh();
}

int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *paned1, *paned2;
	GtkWidget *sw, *sw2, *sw3;
	GtkCssProvider *provider;

	gtk_init(&argc, &argv);

	if( argc <= 1 ) {
		printf("No hosts arguments given\n");
		return 1;
	}
	music_endpoint = g_string_new(argv[1]);

	//Library *libraryDetails;
	//int err = fetch_library( music_endpoint, &libraryDetails );
	//if( err != 0 ) {
	//	printf("failed to fetch library\n");
	//	return 1;
	//}
	//global_library = libraryDetails;

	//err = fetch_playlists( music_endpoint, &global_playlists );
	//if( err != 0 ) {
	//	printf("failed to fetch playlists\n");
	//	return 1;
	//}
	//for( int i = 0; i < global_playlists.num_playlists; i++ ) {
	//	global_playlists.playlists[i].list_store = create_playlist_store( libraryDetails, &global_playlists.playlists[i] );
	//}
	//assert( global_playlists.num_playlists > 0 );

	window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title( GTK_WINDOW(window), "Music playlist" );
	gtk_window_set_default_size( GTK_WINDOW(window), 1400, 800 );

	g_signal_connect( window, "destroy", G_CALLBACK( destroy ), NULL );

	GtkWidget *box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_container_add( GTK_CONTAINER(window), box );

	global_library_tree_store = create_library_store();
	global_playlists_list_store = create_playlists_store();
	global_host_list_store = create_hosts_store();
	populate_hosts( global_host_list_store, (const char**) &argv[1] );

	global_library_widget = create_library_widget( global_library_tree_store );
	global_playlist_widget = create_playlist_widget( global_library_tree_store );
	global_playlist_selector_widget = create_playlist_selector_widget( global_playlists_list_store );
	global_host_selector_widget = gtk_combo_box_new();
	gtk_combo_box_set_model( GTK_COMBO_BOX(global_host_selector_widget), GTK_TREE_MODEL(global_host_list_store) );

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(global_host_selector_widget), renderer, TRUE);
	gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(global_host_selector_widget), renderer,
			"text", 0,
			NULL);

	gtk_combo_box_set_active (GTK_COMBO_BOX (global_host_selector_widget), 0);
	g_signal_connect_swapped( global_host_selector_widget,
			"changed",
			G_CALLBACK( host_selector_changed ),
			NULL );

	provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data( provider, css, -1, NULL );
	gtk_style_context_add_provider_for_screen( gdk_screen_get_default(), GTK_STYLE_PROVIDER( provider ), 800 );

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	sw2 = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw2), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw2), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	sw3 = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw3), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw3), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	drag_target = gtk_target_entry_new("GTK_LIST_BOX_ROW", 0, 0);

	paned1 = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_set_position( GTK_PANED(paned1), 300 );

	paned2 = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_set_position( GTK_PANED(paned2), 800 );

	gtk_paned_pack1( GTK_PANED(paned1), sw, TRUE, FALSE );
	gtk_paned_pack2( GTK_PANED(paned1), paned2, TRUE, FALSE );
	gtk_paned_pack1( GTK_PANED(paned2), sw2, TRUE, FALSE );
	gtk_paned_pack2( GTK_PANED(paned2), sw3, TRUE, FALSE );

	gtk_widget_set_size_request( global_library_widget, 50, -1 );
	gtk_widget_set_size_request( global_playlist_widget, 50, -1);
	gtk_widget_set_size_request( global_playlist_selector_widget, 50, -1);

	gtk_container_add( GTK_CONTAINER( sw ), global_library_widget );
	gtk_container_add( GTK_CONTAINER( sw2 ), global_playlist_widget );
	gtk_container_add( GTK_CONTAINER( sw3 ), global_playlist_selector_widget );

	gtk_box_pack_start( GTK_BOX(box), global_host_selector_widget, FALSE, FALSE, 0);
	gtk_box_pack_start( GTK_BOX(box), paned1, TRUE, TRUE, 0);


	GtkAccelGroup *accel_group = gtk_accel_group_new ();
	gtk_accel_group_connect( accel_group, GDK_KEY_n, GDK_CONTROL_MASK, 0, g_cclosure_new( G_CALLBACK( accelerator_new_playlist ), window, 0) );
	gtk_accel_group_connect( accel_group, GDK_KEY_p, GDK_CONTROL_MASK, 0, g_cclosure_new( G_CALLBACK( accelerator_pause ), window, 0) );
	gtk_accel_group_connect( accel_group, GDK_KEY_r, GDK_CONTROL_MASK, 0, g_cclosure_new( G_CALLBACK( accelerator_refresh ), window, 0) );
	gtk_accel_group_connect( accel_group, GDK_KEY_s, GDK_CONTROL_MASK, 0, g_cclosure_new( G_CALLBACK( accelerator_save ), window, 0) );
	gtk_accel_group_connect( accel_group, GDK_KEY_q, GDK_CONTROL_MASK, 0, g_cclosure_new( G_CALLBACK( destroy ), window, 0) );
	gtk_accel_group_connect( accel_group, GDK_KEY_w, GDK_CONTROL_MASK, 0, g_cclosure_new( G_CALLBACK( destroy ), window, 0) );

    gtk_window_add_accel_group( GTK_WINDOW(window), accel_group );


	gtk_widget_show_all (window);

	refresh();

	gtk_main();
	return 0;
}
