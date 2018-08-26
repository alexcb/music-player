var current_playing = null;
var current_track_id = null;
var current_playlist_checksum = 0;
var activewidget = null;
var tracks_by_path = {};
var library = null;
var playlists = null;
var playlist_widgets = null;
var selected_playlist_index = 0;
var ws = null;

function next_playlist() {
  selected_playlist_index++;
  if( selected_playlist_index >= playlist_widgets.length ) {
    selected_playlist_index = 0;
  }
  activewidget = playlist_widgets[selected_playlist_index];
  activewidget.refresh();
}
function prev_playlist() {
  selected_playlist_index--;
  if( selected_playlist_index < 0 ) {
    selected_playlist_index = playlist_widgets.length - 1;
  }
  activewidget = playlist_widgets[selected_playlist_index];
  activewidget.refresh();
}

function get_playlist_widget_by_name(name) {
  if( playlist_widgets === null ) {
    return null;
  }
  for( var i = 0; i < playlist_widgets.length; i++ ) {
    console.log(playlist_widgets[i].name);
    console.log(name);
    if( playlist_widgets[i].name == name ) {
      return playlist_widgets[i];
    }
  }
  return null;
}

function td(text) {
  var td = $('<td>');
  td.text(text);
  return td;
}

function buildtable(headers, data) {
  var n = data.length;
  var m = headers.length;

  var header = ['<table class="myheader pure-table">'];
  header.push('<thead><tr>');
  for( var i = 0; i < m; i++ ) {
    header.push('<th>' + headers[i] + '</th>');
  }
  header.push('</tr></thead></table>');

  var body = ['<table class="mybody pure-table"><tbody>'];
  

  for( var i = 0; i < n; i++ ) {
    body.push('<tr>');
    for( var j = 0; j < m; j++ ) {
      body.push('<td>' + data[i][j] + '</td>');
    }
    body.push('</tr>');
  }
  body.push('</tbody></table>');

  return {
    'header': $(header.join('')),
    'body': $(body.join(''))
  };
}

function load() {
  tracks_by_path = {};
  $.each(library.albums, function(i, album) {
    $.each(album.tracks, function(i, track) {
      tracks_by_path[track.path] = {
        "track": track,
        "album": album
      };
    });
  });

  console.log('finding default');
  playlist_widgets = [];
  $.each(playlists.playlists, function(i, pl) {
    w = new playlist();
    w.load(pl);
    playlist_widgets.push(w);
  });
  
  if( playlist_widgets.length == 0 ) {
    playlist_widgets.push(playlist(pl));
  }

  selectorwidget.load(library.albums);

  if( !select_current_track() ) {
    activewidget = playlist_widgets[0];
    activewidget.refresh();
  }
}

function showAdd() {
  $('#playlist').empty().append($("<b>select</b>"));
}

function refresh(cb) {
  $.when(
    $.getJSON("/library", function(data) {
        library = data;
    }),
    $.getJSON("/playlists", function(data) {
        playlists = data;
        current_playlist_checksum = data.checksum;
    })
  ).then(function() {
    load();
    if( cb !== undefined ) { cb(); }
  });

}

function playSelectedPlaylistItem() {
  alert("todo play: " + selected_playlist_item);
}


var loadingwidget = {
  refresh: function() {
    console.log('loading refresh');
    $('#playlist').empty().append($("<b>loading...</b>"));
  },
  onkeydown: function (e) {
    return false;
  }
};

var helpwidget = {
  refresh: function() {
    console.log('loading refresh');
    $('#playlist').empty().append($("<p>press... a: add track, left/right to cycle playlists, up/down to select tracks, enter to play</p>"));
  },
  onkeydown: function (e) {
    return false;
  }
};


function trackTable() {
  this.name = 'tracktable';
  this.selected = 0;
  this.rows = [];
  this.headers = ['artist', 'album', 'title', 'length'];
};
trackTable.prototype.refresh = function() {
  var table = buildtable(this.headers, this.rows);

  var sticky = $('<div style="position: fixed; top: 0; width:100%"></div>');
  sticky.append(this.getHeader());

  var body = $('<div></div>');
  body.append(table.body);

  $('#thepage').empty().append(sticky).append(body);

  body.css('margin-top', sticky.height());
};
trackTable.prototype.onkeydown = function(e) {
  switch (e.key) {
      case 'ArrowLeft':
      case 'h':
        prev_playlist();
        return false;
      case 'ArrowUp':
      case 'k':
        this.selectPlaylistItem(-1);
        return false;
      case 'ArrowRight':
      case 'l':
        next_playlist();
        return false;
      case 'ArrowDown':
      case 'j':
        this.selectPlaylistItem(1);
        return false;
      case 'Enter':
        this.onenter();
        return false;
  }
  console.log(e.key);
};

trackTable.prototype.getHeader = function() {
  return $('<b>' + this.name + '</b>');
}

trackTable.prototype.selectPlaylistItem = function(x) {
  $('.mybody tr').eq(this.selected).removeClass("pure-table-odd");
  this.selected += x;
  if( this.selected < 0 ) {
    this.selected = 0;
  }
  if( this.selected >= this.rows.length ) {
    this.selected = this.rows.length - 1;
  }
  $('.mybody tr').eq(this.selected).addClass("pure-table-odd");
  var visible = this.selected-4;
  if( visible < 0 ) { visible = 0; }
  $.scrollTo($('.mybody tr').eq(visible).offset().top-50); // TODO 50 should be the height of the fixed header
}
trackTable.prototype.onenter = function() {
  console.log(this.selected);
}

function format_length(l) {
  var h = Math.floor(l / 60 / 60);
  l -= h*60*60;
  var m = Math.floor(l / 60);
  l -= m*60;
  var l = Math.floor(l);

  if( h > 0 ) {
    return "" + h.toString() + ":" + m.toString().padStart(2, '0') + ":" + l.toString().padStart(2, '0')
  }
  return "" + m.toString() + ":" + l.toString().padStart(2, '0')
}

function playlist() {
  trackTable.call(this);
  this.name = 'default';
  this.paths = [];
  this.ids = [];
  this.headers = ['artist', 'album', 'year', 'title', 'length', 'id'];
  this.has_changed = false;
}
playlist.prototype = Object.create(trackTable.prototype);
playlist.prototype.onkeydown = function(e) {
  switch (e.key) {
    case 'a':
      // add
      activewidget = selectorwidget;
      selectorwidget.playlist = this;
      activewidget.refresh();
      return false;
  }
  return trackTable.prototype.onkeydown.call(this, e);
}
playlist.prototype.add_track = function(path, id) {
  var track = tracks_by_path[path];
  var cols = [];
  cols.push(track.album.artist);
  cols.push(track.album.album);
  cols.push(track.album.year);
  cols.push(track.track.title);
  cols.push(format_length(track.track.length));
  cols.push(id);
  this.rows.push(cols)
  this.paths.push(path);
  this.ids.push(id);
  if( id === undefined ) {
    this.has_changed = true;
  }
}
playlist.prototype.load = function(playlist) {
  this.paths = [];
  this.ids = [];
  this.rows = [];
  this.name = playlist.name;
  for( var i = 0; i < playlist.items.length; i++ ) {
    if( 'stream' in playlist.items[i] ) {
      console.log('unsupported stream');
      console.log(playlist.items[i]);
    } else {
      this.add_track(playlist.items[i].path, playlist.items[i].id);
    }
  }
}
playlist.prototype.getHeader = function() {
  return $('<span>playlist: <b>' + this.name + '</b></span>');
}
playlist.prototype.onenter = function() {
  var selected_index = this.selected;
  var selected_id = this.ids[this.selected];
  var playlist_name = this.name;
  if( this.has_changed ) {
    // update playlist
    var playlist_payload = [];
    for( var i = 0; i < this.paths.length; i++ ) {
      playlist_payload.push([this.paths[i], this.ids[i]]);
    }

    var payload = {
      'name': this.name,
      'playlist': playlist_payload
    };

    $.post( "/playlists", JSON.stringify(payload))
      .done(function( data ) {
        refresh(function(){
          // after the refresh, `this` will no longer point to the correct playlist
          var that = get_playlist_widget_by_name(playlist_name);
          console.log(that);
          console.log(selected_id);
          console.log(selected_index);
          if( selected_id !== undefined ) {
            that.play_track_by_id(selected_id);
          } else {
            that.play_track_by_index(selected_index);
          }
        });
      })
      .fail(function( data ) {
        console.log('failed to post playlist');
        console.log(data);
      });
    return;
  }
  this.play_track_by_id( selected_id );
}
playlist.prototype.play_track_by_id = function(track_id) {
  $.post( "/play_track_id?track=" + track_id + "&when=immediate")
    .done(function(data) {
      console.log('it worked');
      console.log(data);
    })
    .fail(function(data) {
      console.log('it failed');
      console.log(data);
    });
  return;
}
playlist.prototype.play_track_by_index = function(track_index) {
  if( track_index >= this.ids.length ) {
    alert('track_index out of range');
    return;
  }
  var track_id = this.ids[track_index];
  if( track_id > 0 ) {
    this.play_track_by_id( track_id );
  } else {
    alert('track_id is ' + track_id);
  }
}

//playlist.prototype.play_selected = function() {
//    console.log('here');
//    var path = this.paths[this.selected];
//    var id = this.ids[this.selected];
//    $.post( "/play?playlist=default&track=" + this.selected + "&when=immediate")
//      .done(function(data) {
//        console.log('it worked');
//        console.log(data);
//      })
//      .fail(function(data) {
//        console.log('it failed');
//        console.log(data);
//      });
//}

playlist.prototype.refresh = function() {
  trackTable.prototype.refresh.call(this);
  for( var i = 0; i < this.ids.length; i++ ) {
    if( this.ids[i] == current_track_id ) {
      $('.mybody tr').eq(i).addClass("active-track");
    }
  }
}

function selector() {
  trackTable.call(this);
  this.name = 'selector';
  this.playlist = null;
  this.albumview = true;
}
selector.prototype = Object.create(trackTable.prototype);
selector.prototype.load = function(albums) {
  this.albums = albums;
}
selector.prototype.refresh = function() {
  var rows = [];
  var paths = []
  console.log(this.albumview);
  if( this.albumview ) {
    $.each(this.albums, function(i, album) {
      console.log('album view');
      var cols = [];
      cols.push(album.artist);
      cols.push(album.album);
      cols.push(album.year);
      cols.push(album.tracks.length);
      rows.push(cols)
      var album_paths = [];
      $.each(album.tracks, function(i, track) { album_paths.push(track.path); });
      paths.push(album_paths);
    });
  } else {
    $.each(this.albums, function(i, album) {
      console.log('track view');
      $.each(album.tracks, function(i, track) {
        var cols = [];
        cols.push(album.artist);
        cols.push(album.album);
        cols.push(album.year);
        cols.push(track.title);
        cols.push(track.length);
        rows.push(cols)
        paths.push([track.path]);
      });
    });
  }
  this.rows = rows;
  this.paths = paths;

  trackTable.prototype.refresh.call(this);
}
selector.prototype.onkeydown = function(e) {
  switch (e.key) {
    case 'Escape':
      // 'escape' escape
      activewidget = this.playlist;
      activewidget.refresh();
      return false;
    case 't':
      // toggle album/track view
      this.albumview = !this.albumview;
      this.refresh();
      return false;
  }
  return trackTable.prototype.onkeydown.call(this, e);
}
selector.prototype.onenter = function() {
  var paths = this.paths[this.selected];
  for( var i = 0; i < paths.length; i++ ) {
    console.log("adding " + paths[i]);
    this.playlist.add_track(paths[i]);
  }
}

//playlistwidget = new playlist();
selectorwidget = new selector();

function set_current_track(playing, track_id) {
  current_track_id = track_id;
  current_playing = playing;
}

function select_current_track() {
  if( current_track_id == null || playlist_widgets == null ) {
    return false;
  }
  for( var i = 0; i < playlist_widgets.length; i++ ) {
    var w = playlist_widgets[i];
    for( var j = 0; j < w.ids.length; j++ ) {
      var id = w.ids[j];
      if( id == current_track_id ) {
        console.log('setting');
        activewidget = playlist_widgets[i];
        activewidget.refresh();
        return true;
      }
    }
  }
  return false;
}

function playmethehits() {
  activewidget = loadingwidget;
  activewidget.refresh();
  refresh();

  ws = new WebSocket(((window.location.protocol === "https:") ? "wss://" : "ws://") + window.location.host + "/websocket");
  ws.onmessage = function (event) {
    msg = JSON.parse( event.data );
    console.log('websocket message');
    console.log(msg);
    if( msg.type == "status" ) {
      set_current_track( msg.playing, msg.id );
      if( msg.playlist_checksum != current_playlist_checksum ) {
        refresh();
      } else {
        select_current_track();
      }
    }
  }

  $(document).on("keydown", function(ev) {
    if( ev.ctrlKey || ev.altKey ) {
      return true;
    }
    if( ev.key == '?' ) {
      alert('left/right: change playlists; up/down select tracks; a: display add-track; escape: display playlist; enter: select/play song');
      return
    }
    return activewidget.onkeydown(ev);
  });
}


$(window).on('load', function () {
    playmethehits();
});
