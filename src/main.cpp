#include <account/pages/pages.h>
#include <account/session.h>
#include <core/cgi.h>
#include <core/db.h>
#include <log/log.h>
#include <misc/timer.h>
#include <pages/pages.h>
#include <playlist/pages/pages.h>
#include <render/document.h>
#include <render/fcgiio.h>
#include <social/pages/pages.h>
#include <stat/pages.h>
#include <text/modifiers.h>
#include <text/text.h>
#include <track/pages/pages.h>
#include <userfeature/pages/pages.h>
#include <youtube/pages/pages.h>

#include <fcntl.h>
#include <wand/magick_wand.h>
#include <signal.h>
#include <stdexcept>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <syslog.h>

bool running = true, answering = false;

void signalCatch(int s){
    if(s){
        if(answering)
            running = false;
        else
            DB::close();
            exit(0);
    }
}

int main(int argc, char** argv){
    (void)argc;
    (void)argv;

    setenv("EQBEATS_DIR", DEFAULT_EQBEATS_DIR, 0);

    openlog("eqbeats", LOG_PID, LOG_USER);

    DB::connect((std::string)argv[0] + "-" + number(getpid()));

    uint32_t buf = 0;
    int urandom = open("/dev/urandom", O_RDONLY);
    if(urandom != -1) {
        read(urandom, &buf, 32);
        close(urandom);
        srand(buf);
    } else {
        log("Couldn't open /dev/urandom, random seed will be predictable.");
        srand(getpid() ^ time(NULL));
    }

    MagickWandGenesis();

    FCGX_Request request;
    FCGX_Init();
    FCGX_InitRequest(&request, 0, 0);
    FCgiIO o;

    ctemplate::AddModifier("x-irc", new IrcEscape);
    ctemplate::AddXssSafeModifier("x-email", new EmailEscape);
    ctemplate::AddXssSafeModifier("x-format", new Formatter);
    cache.SetTemplateRootDirectory(TEMPLATES_DIR);

    void (*callbacks[])(Document*) = {
        Pages::statics, Pages::home,
        Pages::track, Pages::trackMisc, Pages::tracks, Pages::oembed, Pages::trackActions, Pages::trackUpload, Pages::art, Pages::license,
        Pages::account, Pages::login, Pages::user, Pages::users, Pages::registration, Pages::passwordReset,
        Pages::playlist, Pages::playlistActions,
        Pages::comment, Pages::socialActions, Pages::favorites,
        Pages::featureActions,
        Pages::oauth, Pages::yt_upload,
        Pages::stats,
        0
    };

    struct sigaction sa;
    sa.sa_handler = &signalCatch;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    pid_t child;
    int rc;

    while(running && (FCGX_Accept_r(&request) == 0)){
        answering = true;
        resetTimer();
        DB::healthCheck();
        headers = request.envp;
        o.attach(&request);
        try { cgi = cgicc::Cgicc(&o); }
        catch(std::runtime_error &err){
            log((std::string)"CgiCc error: " + err.what());
            //continue;
        }
        path = stripSlash(cgi.getEnvironment().getScriptName());
        Document doc;

        // Nope
        if (cgi.getElementByValue("PHPE9568F34-D428-11d2-A769-00AA001ACF42") != cgi.getElements().end() ||
            cgi.getElementByValue("PHPE9568F35-D428-11d2-A769-00AA001ACF42") != cgi.getElements().end() ||
            cgi.getElementByValue("PHPE9568F36-D428-11d2-A769-00AA001ACF42") != cgi.getElements().end()){
            doc.redirect("http://youtu.be/gvdf5n-zI14");
        }

        if (getenv("EQBEATS_HTTPS") && !cgi.getEnvironment().usingHTTPS() && cgi.getElements().size() == 0)
            doc.moved(eqbeatsUrl() + path);

        Session::start();

        for(int i=0; !doc && callbacks[i]; i++)
            callbacks[i](&doc);

        Session::fill(doc.rootDict());
        o << doc.generate();

        Session::destroy();
        FCGX_Finish_r(&request);
        while((child = waitpid(-1, &rc, WNOHANG)) > 0) // wait for zombies
            syslog(WIFEXITED(rc) ? LOG_NOTICE : LOG_ERR, "%d exited with %d.", child, WEXITSTATUS(rc));
        answering = false;
    }

    MagickWandTerminus();
    DB::close();
    return 0;
}
