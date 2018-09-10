'use strict';

var ws;
var active_widget;
var selector;
var playlist_manager;
var current_playlist_checksum;
var tracks_by_path = {};
var current_track_id;
var last_navigation = null;
var playing = null;
var current_playing_str = "";

function refresh_tracks_by_path(library) {
  tracks_by_path = {};
  $.each(library.albums, function(i, album) {
    $.each(album.tracks, function(i, track) {
      tracks_by_path[track.path] = {
        "track": track,
        "album": album
      };
    });
  });
}

function set_current_track( is_playing, artist, track, duration, track_id ) {
  playing = is_playing;
  console.log(playing);
  console.log(artist);
  current_track_id = track_id;
  current_playing_str = artist + ' - ' + track + ' (' + format_length(duration) + ')';
  if( active_widget ) {
    active_widget.refresh();
  }
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

function setup_websocket() {
  ws = new WebSocket(((window.location.protocol === "https:") ? "wss://" : "ws://") + window.location.host + "/websocket");
  ws.onmessage = function (event) {
    var msg = JSON.parse( event.data );
    console.log('websocket message');
    console.log(msg);
    if( msg.type == "status" ) {
      set_current_track( msg.playing, msg.artist, msg.title, msg.length, msg.id );
      if( msg.playlist_checksum != current_playlist_checksum ) {
        refreshLibraryAndPlaylists(function() {
          playlist_manager.scrollToTrack(current_track_id);
        });
      } else {
        console.log(msg);
        //select_current_track();
      }
    }
  }
}

function resize_elements() {
  var h = window.innerHeight;
  $('#thefooter').css('top', h - $('#thefooter').outerHeight());
  $('#thebody').css('margin-top', $('#theheader').outerHeight());
  $('#thebody').css('margin-bottom', $('#thefooter').outerHeight());
}

function ensure_default_playlist(playlists) {
  for( var i = 0; i < playlists.length; i++ ) {
    if( playlists[i].name == 'default' ) {
      return playlists;
    }
  }
  playlists.push({
    'name': 'default',
    'items': [],
    'id': 0
  });
  return playlists;
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

// from https://github.com/bevacqua/fuzzysearch/blob/master/index.js
function fuzzysearch(needle, haystack) {
  var hlen = haystack.length;
  var nlen = needle.length;
  if (nlen > hlen) {
    return false;
  }
  if (nlen === hlen) {
    return needle === haystack;
  }
  outer: for (var i = 0, j = 0; i < nlen; i++) {
    var nch = needle.charCodeAt(i);
    while (j < hlen) {
      if (haystack.charCodeAt(j++) === nch) {
        continue outer;
      }
    }
    return false;
  }
  return true;
}

function search_match(filter, s) {
  //console.log('search if ' + filter + ' is in ' + s);
  if( filter == "" ) {
    return true;
  }
  return fuzzysearch(filter.toLowerCase(), s.toLowerCase());
}

function PlaylistManager() {
  this.playlists = [];
  console.log('init');
  this.selected_playlist = 0;
  this.selected_playlist_item = [];
  this.search = "";
  this.visible_tracks = null;
};
PlaylistManager.prototype.load = function(playlists) {
  this.playlists = playlists;
  this.selected_playlist = 0;
  this.selected_playlist_item = []
  this.has_changed = []
  for( var i = 0; i < playlists.length; i++ ) {
    this.selected_playlist_item.push(0);
    this.has_changed.push(false);
  }
}
PlaylistManager.prototype.select_playlist_by_name = function(name) {
  for( var i = 0; i < this.playlists.length; i++ ) {
    if( this.playlists[i].name == name ) {
      this.selected_playlist = i;
      return true;
    }
  }
  return false;
}
PlaylistManager.prototype.get_selected_name = function() {
  return this.playlists[this.selected_playlist]['name'];
}
PlaylistManager.prototype.add_track = function(path) {
  this.playlists[this.selected_playlist]['items'].push({'path': path});
  this.has_changed[this.selected_playlist] = true;
}
PlaylistManager.prototype.refresh = function() {
  if( this.playlists.length == 0 ) {
    return;
  }
  console.log('refresh!' + this.selected_playlist);
  var rows = []
  var playlist_name = this.playlists[this.selected_playlist]['name'];
  var playlist = this.playlists[this.selected_playlist]['items'];
  var visible_tracks = [];
  var active_row = null;
  for( var i = 0; i < playlist.length; i++ ) {
    if( playlist[i].path !== undefined ) {
      var track = tracks_by_path[playlist[i].path];
      if( search_match(this.search, track.album.artist) ||
          search_match(this.search, track.album.album)  ||
          search_match(this.search, track.track.title) ) {
        var cols = [];
        cols.push(track.album.artist);
        cols.push(track.album.album);
        cols.push(track.album.year);
        cols.push(track.track.title);
        cols.push(format_length(track.track.length));
        cols.push(playlist[i].id);
        rows.push(cols);
        visible_tracks.push(i);
        if( current_track_id !== null && current_track_id == playlist[i].id ) {
          active_row = i;
        }
      }
    } else if( playlist[i].stream !== undefined ) {
      console.log('stream not supported');
    }
  }
  var headers = ['artist', 'album', 'year', 'title', 'duration', 'id'];
  var table = buildtable(headers, rows);
  $('#thebody').empty().append(table.body);
  if( active_row !== null ) {
    $('.mybody tr').eq(active_row).addClass("active-track");
  }
  this.visible_tracks = visible_tracks;

  var playlist_manager = this;
  $('.mybody tr').each(function(i) {
    //$(this).bind( "mouseenter", function() {
    //  if( last_navigation == 'mouse' ) {
    //    playlist_manager.set_playlist_item(i);
    //  }
    //  last_navigation = 'mouse';
    //});
    $(this).bind( "click", function() {
      playlist_manager.set_playlist_item(i);
      playlist_manager.onenter();
    });
  });

  var num = this.selected_playlist + 1;
  var total = this.playlists.length;
  var header = $('<span>');

  var leftheader = $('<span class="headerplaylist">Playlist: ' + playlist_name + ' (' + num + '/' + total + ')</span>');
  leftheader.bind('click', function() {
    playlist_manager.next_playlist(1);
  });
  header.append(leftheader);


  var prev = $('<i class="fa fa-backward"></li>');
  prev.bind( 'click', function() { playlist_manager.prev_track(); });
  var next = $('<i class="fa fa-forward"></li>');
  next.bind( 'click', function() { playlist_manager.next_track(); });
  var playpause;
  if( playing ) {
    playpause = $('<i class="fa fa-pause"></i>');
    playpause.bind( 'click', function() {
      playlist_manager.pause();
    });
  } else {
    playpause = $('<i class="fa fa-play"></i>');
    playpause.bind( 'click', function() {
      playlist_manager.play();
    });
  }

  var current_status = $('<span>' + current_playing_str + '</span>');
  current_status.bind( 'click', function() {
    playlist_manager.scrollToTrack(current_track_id);
  });

  var x = $('<span class="headercontrol">');
  x.append(prev);
  x.append($('<span>&nbsp;&nbsp;</span>'));
  x.append(playpause);
  x.append($('<span>&nbsp;&nbsp;</span>'));
  x.append(next);
  x.append($('<span>&nbsp;&nbsp;</span>'));
  x.append(current_status);

  header.append(x);

  $('#theheader').empty().append(header);

  var add_to = $('<a>add to current playlist</a>');
  add_to.bind( 'click', function() {
    active_widget = selector;
    active_widget.refresh();
  });

  var new_playlist = $('<a>new playlist</a>');
  new_playlist.bind( 'click', function() {
    var playlist_name = prompt("enter new playlist name", "new playlist");
    playlist_manager.new_playlist(playlist_name);
  });

  $('#footerlink').empty().append(add_to).append('<span>&nbsp;&nbsp;&nbsp;</span>').append(new_playlist);
};
PlaylistManager.prototype.new_playlist = function(playlist_name) {
  for( var i = 0; i < this.playlists.length; i++ ) {
    if( this.playlists[i].name == playlist_name ) {
      this.selected_playlist = i;
      return
    }
  }
  this.playlists.push({
    'name': playlist_name,
    'items': [],
    'id': 0
  });
  this.selected_playlist = this.playlists.length - 1;
  this.refresh();
}
PlaylistManager.prototype.play = function(playlist_name, selected_index) {
  this.pause();
}
PlaylistManager.prototype.pause = function(playlist_name, selected_index) {
  $.post( "/pause")
    .done(function( data ) {
      console.log('done pause');
    })
    .fail(function( data ) {
      console.log('failed to post pause');
      console.log(data);
    });
}
PlaylistManager.prototype.get_track_id_by_position = function(playlist_name, selected_index) {
  for( var i = 0; i < this.playlists.length; i++ ) {
    if( this.playlists[i].name == playlist_name ) {
      console.log(selected_index);
      console.log(this.playlists[i]);
      console.log(this.playlists[i].items[selected_index]);
      return this.playlists[i].items[selected_index].id;
    }
  }
}
PlaylistManager.prototype.prev_track = function() {
  $.post( "/prev-track")
    .done(function( data ) {
      console.log('done prev track');
    })
    .fail(function( data ) {
      console.log('failed to post prev-track');
      console.log(data);
    });
}
PlaylistManager.prototype.next_track = function() {
  $.post( "/next-track")
    .done(function( data ) {
      console.log('done next track');
    })
    .fail(function( data ) {
      console.log('failed to post next-track');
      console.log(data);
    });
}
PlaylistManager.prototype.next_playlist = function(direction) {
  this.selected_playlist += direction;
  if( this.selected_playlist >= this.playlists.length ) {
    this.selected_playlist = 0;
  } else if( this.selected_playlist < 0 ) {
    this.selected_playlist = this.playlists.length - 1;
  }
  console.log(this.selected_playlist);
  this.refresh();
}
PlaylistManager.prototype.scroll_to_selected_item = function() {
  var i = this.selected_playlist_item[this.selected_playlist];
  var visible = i-4;
  if( visible < 0 ) { visible = 0; }
  var new_y = $('.mybody tr').eq(visible).offset().top-50;
  if( new_y < 0 ) {
    new_y = 0;
  }
  $.scrollTo(new_y);
}
PlaylistManager.prototype.scrollToTrack = function(track_id) {
  var index = this.get_track_by_id(track_id);
  if( index === null ) {
    console.log('failed to scrollToTrack');
    return;
  }
  console.log('scrollToTrack');
  this.search = '';
  this.selected_playlist = index['playlist'];
  this.selected_playlist_item[index['playlist']] = index['item'];
  this.refresh();

  var i = index['item'];
  var visible = i-4;
  if( visible < 0 ) { visible = 0; }
  var new_y = $('.mybody tr').eq(visible).offset().top-50;
  if( new_y < 0 ) {
    new_y = 0;
  }
  console.log('scrolling to ' + new_y);
  $.scrollTo(new_y);

}
PlaylistManager.prototype.get_track_by_id = function(track_id) {
  for( var i = 0; i < this.playlists.length; i++ ) {
    for( var j = 0; j < this.playlists[i].items.length; j++ ) {
      if( this.playlists[i].items[j].id == track_id ) {
        return {'playlist': i, 'item': j};
      }
    }
  }
  return null;
}
PlaylistManager.prototype.set_playlist_item_delta = function(x) {
  last_navigation = 'key';
  if( this.selected_playlist >= this.selected_playlist_item.length ) {
    return;
  }
  var i = this.selected_playlist_item[this.selected_playlist];

  var before = $('.mybody tr').eq(i);
  if( before.length <= 0 ) {
    return;
  }
  console.log(before);
  var before_y = before.offset().top;

  this.set_playlist_item(i + x);

  i = this.selected_playlist_item[this.selected_playlist];
  var after = $('.mybody tr').eq(i);
  if( after.length <= 0 ) {
    return;
  }
  var after_y = after.offset().top;

  var diff = after_y - before_y;
  var y = $(document).scrollTop() + diff;
  if( y < 0 ) { y = 0; }
  $.scrollTo(y);
}
PlaylistManager.prototype.set_playlist_item = function(x) {
  var rows = $('.mybody tr')
  var num_visible_rows = rows.length;
  if( num_visible_rows == 0 ) {
    return;
  }

  var i = this.selected_playlist_item[this.selected_playlist];

  rows.eq(i).removeClass("pure-table-odd");
  i = x;
  if( i < 0 ) {
    i = 0;
  }
  if( i >= num_visible_rows ) {
    i = num_visible_rows - 1;
  }
  $('.mybody tr').eq(i).addClass("pure-table-odd");
  this.selected_playlist_item[this.selected_playlist] = i;
}
PlaylistManager.prototype.onenter = function() {
  var playlist_name = this.playlists[this.selected_playlist].name;
  var item_index = this.visible_tracks[this.selected_playlist_item[this.selected_playlist]];
  var track_id = this.playlists[this.selected_playlist].items[item_index].id;
  var has_changed = this.has_changed[this.selected_playlist];
  if( has_changed ) {
    // update playlist
    var playlist_payload = [];
    var items = this.playlists[this.selected_playlist].items;
    for( var i = 0; i < items.length; i++ ) {
      var path = items[i].path;
      var id = items[i].id;
      if( id === undefined ) {
        id = 0;
      }
      playlist_payload.push([path, id]);
    }

    var payload = {
      'name': this.playlists[this.selected_playlist].name,
      'playlist': playlist_payload
    };

    var that = this;
    $.post( "/playlists", JSON.stringify(payload))
      .done(function( data ) {
        refreshLibraryAndPlaylists(function(){
          if( track_id === undefined ) {
            track_id = that.get_track_id_by_position(playlist_name, item_index);
            if( !track_id ) {
              console.log('failed to lookup track id');
            }
          }
          play_track_by_id(track_id);
        });
      })
      .fail(function( data ) {
        console.log('failed to post playlist');
        console.log(data);
      });
    return;
  }
  play_track_by_id( track_id );
}
function play_track_by_id(track_id) {
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
function play_track_by_index(playlist_name, selected_index) {
  for( var i = 0; i < playlist_manager.playlists.length; i++ ) {
    if( playlist_manager.playlists[i].name == playlist_name ) {
      return playlists;
    }
  }

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
PlaylistManager.prototype.onkeydown = function(e) {
  console.log('in playlist manager onkeydown');
  switch (e.key) {
      case 'ArrowLeft':
      case 'h':
        this.next_playlist(-1);
        return false;
      case 'ArrowUp':
      case 'k':
        this.set_playlist_item_delta(-1);
        return false;
      case 'ArrowRight':
      case 'l':
        this.next_playlist(1);
        return false;
      case 'ArrowDown':
      case 'j':
        this.set_playlist_item_delta(1);
        return false;
      case 'Enter':
        this.onenter();
        return false;
      case 'a':
        active_widget = selector;
        active_widget.refresh();
        return false;
  }
  console.log(e.key);
};

function Selector() {
  this.albums = [];
  this.streams = [];
  this.view = 'album';
  this.selected = 0;
  this.search = '';
};
Selector.prototype.load = function(library) {
  this.albums = library.albums;
  this.streams = library.streams;
}
Selector.prototype.refresh = function() {
  var playlist_name = playlist_manager.get_selected_name();
  $('#theheader').empty().append($('<span>Select item to add to ' + playlist_name + '</span>'));

  var back = $('<a>back to playlist</a>');
  back.bind( 'click', function() {
    active_widget = playlist_manager;
    active_widget.refresh();
  });
  $('#footerlink').empty().append(back);

  var rows = [];
  var paths = []
  if( this.view == 'album' ) {
    for( var i = 0; i < this.albums.length; i++ ) {
      var album = this.albums[i];
      if( search_match(this.search, album.artist) ||
          search_match(this.search, album.album) ) {
        var cols = [];
        cols.push(album.artist);
        cols.push(album.album);
        cols.push(album.year);
        cols.push(album.tracks.length);
        rows.push(cols)
        var album_paths = [];
        $.each(album.tracks, function(i, track) { album_paths.push(track.path); });
        paths.push(album_paths);
      }
    }

    var headers = ['artist', 'album', 'year', 'num tracks'];
    var table = buildtable(headers, rows);
    $('#thebody').empty().append(table.body);
    this.paths = paths;

    var that = this;
    $('.mybody tr').each(function(ii) {
      $(this).bind( "click", function() {
        console.log(ii);
        var paths_to_add = paths[ii];
        for( var i = 0; i < paths_to_add.length; i++ ) {
          playlist_manager.add_track(paths_to_add[i]);
        }
      active_widget = playlist_manager;
      active_widget.refresh();
      });
    });

    return;
  }

  if( this.view == 'track' ) {
    $.each(this.albums, function(i, album) {
      $.each(album.tracks, function(i, track) {
        var cols = [];
        cols.push(album.artist);
        cols.push(album.album);
        cols.push(album.year);
        cols.push(track.title);
        cols.push(format_length(track.length));
        rows.push(cols)
        paths.push([track.path]);
      });
    });

    var headers = ['artist', 'album', 'year', 'track', 'duration'];
    var table = buildtable(headers, rows);
    $('#thebody').empty().append(table.body);
    this.paths = paths;
    return;
  }
  console.log('unsupported');
};
Selector.prototype.toggle_view = function() {
  var ordering = ['track', 'album'] //, 'stream'];
  var found = false;
  for( var i = 0; i < ordering.length; i++ ) {
    if( ordering[i] == this.view ) {
      this.view = ordering[(i+1)%ordering.length];
      found = true;
      break;
    }
  }
  if( !found ) {
    this.view = ordering[0];
  }
  console.log('view set to ' + this.view);
  this.refresh();
}
Selector.prototype.onenter = function() {
  var paths = this.paths[this.selected];
  for( var i = 0; i < paths.length; i++ ) {
    playlist_manager.add_track(paths[i]);
  }
}
Selector.prototype.onkeydown = function(e) {
  console.log('in selector onkeydown');
  switch (e.key) {
      case 'ArrowUp':
      case 'k':
        this.select_item(-1);
        return false;
      case 'ArrowDown':
      case 'j':
        this.select_item(1);
        return false;
      case 't':
        this.toggle_view();
        return false;
      case 'Enter':
        this.onenter();
        return false;
      case 'Escape':
        active_widget = playlist_manager;
        active_widget.refresh();
        return false;
  }
  console.log(e.key);
};
Selector.prototype.select_item = function(x) {
  var rows = $('.mybody tr')
  var num_visible_rows = rows.length;
  if( num_visible_rows == 0 ) {
    return;
  }

  var i = this.selected;

  rows.eq(i).removeClass("pure-table-odd");
  i += x;
  if( i < 0 ) {
    i = 0;
  }
  if( i >= num_visible_rows ) {
    i = num_visible_rows - 1;
  }
  $('.mybody tr').eq(i).addClass("pure-table-odd");
  var visible = i-4;
  if( visible < 0 ) { visible = 0; }
  $.scrollTo($('.mybody tr').eq(visible).offset().top-50); // TODO 50 should be the height of the fixed header
  this.selected = i;
}

function refreshLibraryAndPlaylists(cb) {
  var library;
  var playlists;
  $.when(
    $.getJSON("/library", function(data) {
        library = data;
    }),
    $.getJSON("/playlists", function(data) {
        playlists = data.playlists;
        current_playlist_checksum = data.checksum;
    })
  ).then(function() {
    refresh_tracks_by_path(library);
    //
    selector.load(library);
    playlist_manager.load(ensure_default_playlist(playlists));
    playlist_manager.refresh();
    //load();
    if( cb !== undefined ) { cb(); }
  });
}

$(window).on('load', function () {
  setup_websocket();
  resize_elements();
  $(window).on("resize", resize_elements);

  selector = new Selector();
  playlist_manager = new PlaylistManager();
  active_widget = playlist_manager;

  $('#thesearch').on('input',function(e){
    active_widget.search = this.value;
    active_widget.refresh();
  });
  $('#thesearch').on('keydown',function(e){
    if( e.key == 'Escape' ) {
      $('#thebody').focus();
      return false;
    }
    if( e.key == 'ArrowUp' || e.key == 'ArrowDown' || e.key == 'Enter' ) {
      active_widget.onkeydown(e);
      return false;
    }
    return true;
  });

  $('#thebody').on('keydown', function(e) {
    if( e.key == '/' ) {
      $('#thesearch').focus();
      return false;
    }
    return active_widget.onkeydown(e);
  });

  $('#thebody').focus();

  $(document).on('keydown', function(e) {
    var active = document.activeElement;
    if( active && active.id != "thebody" && active.id != "thesearch" ) {
      console.log('document received a keydown event; sending focus to #thebody');
      $('#thebody').focus();
    }
  });

  refreshLibraryAndPlaylists(function() {
    playlist_manager.scrollToTrack(current_track_id);
  });
});
