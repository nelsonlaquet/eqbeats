#include "event.h"
#include "db.h"
#include "number.h"
#include "session.h"

Event::Event(Type nType, User nSource, User nTarget, Track nTrack, std::string nMessage){
    _type = nType;
    _target = nTarget;
    _source = nSource;
    _track = nTrack;
    _message = nMessage;
}

void Event::push(){
    DB::query((std::string)
        "INSERT INTO events (type, target_id, target_name, source_id, source_name, track_id, track_title, message, date)"
        "VALUES ("
            "'" + (_type==Publish?"publish":_type==Comment?"comment":_type==Favorite?"favorite":"follow") + "'," +
            number(_target.id()) + ","
            "$1," +
            number(_source.id()) + ","
            "$2," +
            number(_track.id()) + ","
            "$3, "
            "$4, 'now')",
        _target.name(), _source.name(), _track.title(), _message);

}

void Event::publish(const Track &t){
    Event(Publish, t.artist(), User(), t).push();
}
void Event::favorite(const Track &t, const User &src){
    Event(Favorite, src, t.artist(), t).push();
}
void Event::follow(const User &u, const User &src){
    Event(Follow, src, u).push();
}
void Event::comment(const Track &t, const User &src, std::string msg){
    Event(Comment, src, t.artist(), t, msg).push();
}
void Event::comment(const User &tgt, const User &src, std::string msg){
    Event(Comment, src, tgt, Track(), msg).push();
}

std::vector<Event> Event::select(std::string cond){
    DB::Result r = DB::query("SELECT type, source_id, source_name, target_id, target_name, track_id, track_title, message FROM events" + (cond.empty()?"":" WHERE "+cond) + " ORDER BY date DESC");
    std::vector<Event> events(r.size());
    for(unsigned i=0; i<r.size(); i++){
        Type t = r[i][0] == "publish"  ? Publish
             : r[i][0] == "comment"  ? Comment
             : r[i][0] == "favorite" ? Favorite : Follow;
        events[i] = Event(t, User(number(r[i][1]),r[i][2]), User(number(r[i][3]),r[i][4]), Track(number(r[i][5]),r[i][6]), r[i][7]);
    }
    return events;
}

std::vector<Event> Event::userEvents(const User &u){
    return select("source_id = " + number(u.id()) + " OR target_id = " + number(u.id())
        + (u == Session::user() ?  "" : " AND type = 'comment'"));
}

std::vector<Event> Event::trackEvents(const Track &t){
    return select("type = 'comment' AND track_id = " + number(t.id()));
}
