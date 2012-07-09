#include "static.h"
#include "document.h"
#include <core/cgi.h>

void staticPages(Document *doc){
    if(path == "/quickstart")
        doc->setHtml("html/quickstart.tpl", "Thanks");
    else if(path == "/faq")
        doc->setHtml("html/faq.tpl", "FAQ");
    else if(path == "/credits")
        doc->setHtml("html/credits.tpl", "Credits");
    else if(path == "/api")
        doc->setHtml("html/api.tpl", "API");
}
