#include "cosuwindow.h"
#include "tools.h"
#include "cosuplatform.h"
#include <cstdio>
#include <cmath>
#include <thread>
#include <climits>
#include <cstdlib>
#include <FL/Fl_JPEG_Image.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/fl_ask.H>

using namespace std;

CosuWindow::CosuWindow() : fr(this)
{
}

double CosuWindow::get_relative_speed()
{
    if (cosuui.rate->value() >= 1)
        return cosuui.speedval->value();
    else if (cosuui.bpm->value() >= 1)
        return cosuui.speedval->value() / fr.info->maxbpm;

    return 1;
}

char bpmstr[] = "xxxxxxxxxxxxxxbpm";

void CosuWindow::update_rate_bpm()
{
    if (fr.info == NULL)
        return;

    double bpm = fr.info->maxbpm * cosuui.speedval->value();
    snprintf(bpmstr, sizeof(bpmstr), "%.0lfbpm", bpm);

    cosuui.ratebpm->label(bpmstr);
}

char arstr[] = "Scale AR to xx.x";
char odstr[] = "Scale OD to xx.x";
const int offset = 12;

void CosuWindow::update_ar_label()
{
    if (fr.info == NULL)
        return;

    if (fr.info->mode == 1 || fr.info->mode == 3)
        return;

    double scaled = scale_ar(cosuui.arslider->value(), get_relative_speed(), fr.info->mode);
    CLAMP(scaled, 0, 11);

    snprintf(arstr + offset, sizeof(arstr) - offset, "%.1lf", scaled);
    cosuui.scale_ar->label(arstr);
}

void CosuWindow::update_od_label()
{
    if (fr.info == NULL)
        return;

    if (fr.info->mode == 2)
        return;

    double scaled = scale_od(cosuui.odslider->value(), get_relative_speed(), fr.info->mode);
    CLAMP(scaled, 0, 11.11);

    snprintf(odstr + offset, sizeof(odstr) - offset, "%.1lf", scaled);
    cosuui.scale_od->label(odstr);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void CosuWindow::rateradio_callb(Fl_Widget *w, void *data)
{
    CosuWindow *win = (CosuWindow*) data;
    win->cosuui.lock->clear();
    if (((Fl_Round_Button*) w)->value() >= 1)
    {
        win->cosuui.bpm->clear();
        win->cosuui.ratebpm->show();
        win->cosuui.lock->hide();
        win->cosuui.speedval->value(1);
        win->cosuui.speedval->step(0.1);

        win->update_ar_label();
        win->update_od_label();
        win->update_rate_bpm();
    }
}

void CosuWindow::bpmradio_callb(Fl_Widget *w, void *data)
{
    CosuWindow *win = (CosuWindow*) data;
    win->cosuui.lock->clear();
    if (((Fl_Round_Button*) w)->value() >= 1)
    {
        win->cosuui.rate->clear();
        win->cosuui.lock->show();
        win->cosuui.ratebpm->hide();
        win->cosuui.speedval->step(1);
        if (win->fr.info != NULL)
        {
            win->cosuui.speedval->value(win->fr.info->maxbpm);
        }
        else
        {
            win->cosuui.speedval->value(1);
        }

        win->update_ar_label();
        win->update_od_label();
    }
}

void CosuWindow::speedval_callb(Fl_Widget *w, void *data)
{
    CosuWindow *win = (CosuWindow*) data;
    if (win->cosuui.rate->value() >= 1)
    {
        win->update_rate_bpm();
    }

    win->update_ar_label();
    win->update_od_label();
}

void CosuWindow::resetbtn_callb(Fl_Widget *w, void *data)
{
    CosuWindow *win = (CosuWindow*) data;
    win->cosuui.hplock->clear();
    win->cosuui.cslock->clear();
    win->cosuui.arlock->clear();
    win->cosuui.odlock->clear();
    win->cosuui.lock->clear();
    win->cosuui.scale_ar->set();
    win->cosuui.scale_od->set();
    win->cosuui.pitch->clear();
    win->cosuui.nospinner->clear();
    win->cosuui.flipbox->value(0);
    win->cosuui.cutstart->value("");
    win->cosuui.cutend->value("");
    win->fr.consumed = false;
    Fl::awake();
}

void CosuWindow::update_progress(void *data, float progress)
{
    CosuWindow *win = (CosuWindow*) data;
    float current = win->cosuui.progress->value();
    if (progress > 0 && progress - current > 0.005)
    {
        win->cosuui.progress->value(progress);
        Fl::check();
    }
    else if (progress == 0 && current == 0)
    {
        // audio didn't provide its size, just update ui in this case.
        Fl::check();
    }
}

static long read_cut_str(const char *str, bool *combo)
{
    char *middle = NULL;
    long cut = strtol(str, &middle, 10);

    if (middle != NULL && *middle == ':')
    {
        cut *= 60;
        cut += strtol(++middle, NULL, 10);
        cut *= 1000;
        *combo = false;
    }
    else
    {
        *combo = true;
    }

    return cut;
}

void CosuWindow::convbtn_callb(Fl_Widget *w, void *data)
{
    CosuWindow *win = (CosuWindow*) data;
    if (win->fr.info == NULL)
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lck(win->fr.mtx);

    win->cosuui.mainbox->deactivate();

    struct editdata edit;

    const char *cutstart_str = win->cosuui.cutstart->value();
    const char *cutend_str = win->cosuui.cutend->value();
    bool start_valid = *cutstart_str != '\0';
    bool end_valid = *cutend_str != '\0';

    edit.cut_combo = false;
    edit.cut_start = 0;
    edit.cut_end = LONG_MAX;

    if (start_valid || end_valid)
    {
        bool start_combo;
        long cutstart = read_cut_str(cutstart_str, &start_combo);

        bool end_combo;
        long cutend = read_cut_str(cutend_str, &end_combo);
        if (cutend == 0)
            cutend = LONG_MAX;

        if (start_valid && end_valid && start_combo != end_combo)
        {
            fl_alert("Cut format is not the same! Not converting.");
            win->cosuui.mainbox->activate();
            return;
        }
        else
        {
            if (cutstart >= cutend)
            {
                fl_alert("Given cut parameter is not valid! Not converting.");
                win->cosuui.mainbox->activate();
                return;
            }
            else
            {
                edit.cut_combo = start_valid ? start_combo : end_combo;
                edit.cut_start = cutstart;
                edit.cut_end = cutend;
            }
        }
    }

    edit.mi = win->fr.info;
    edit.hp = win->cosuui.hpslider->value();
    edit.cs = win->cosuui.csslider->value();
    edit.ar = win->cosuui.arslider->value();
    edit.od = win->cosuui.odslider->value();
    edit.scale_ar = win->cosuui.scale_ar->value() >= 1;
    edit.scale_od = win->cosuui.scale_od->value() >= 1;
    edit.makezip = true;

    edit.speed = win->cosuui.speedval->value();
    edit.bpmmode = win->cosuui.bpm->value() >= 1 ? bpm : rate;
    edit.pitch = win->cosuui.pitch->value() >= 1;

    if (edit.mi->mode != 3)
    {
        edit.nospinner = win->cosuui.nospinner->value() >= 1;
        edit.remove_sv = false;
    }
    else
    {
        edit.remove_sv = win->cosuui.nospinner->value() >= 1;
        edit.nospinner = false;
    }

    switch (win->cosuui.flipbox->value())
    {
    case 1:
        edit.flip = xflip;
        break;
    case 2:
        edit.flip = yflip;
        break;
    case 3:
        edit.flip = transpose;
        break;
    case 4:
        edit.flip = invert;
        break;
    default:
        edit.flip = none;
        break;
    }

    edit.data = data;
    edit.progress_callback = update_progress;

    int ret = edit_beatmap(&edit);
    if (ret != 0)
    {
        fl_alert("Editing a beatmap failed: %d\nCheck console for details.", ret);
    }

    win->cosuui.mainbox->activate();
    win->cosuui.progress->value(0);
}

void CosuWindow::diffch_callb(Fl_Widget *w, void *data)
{
    CosuWindow *win = (CosuWindow*) data;
    int changed = win->cosuui.diffgroup->find(w);
    if (changed >= 8) return;

    int target = changed < 4 /*0,1,2,3 sliders*/ ? changed + 4 : /*4,5,6,7 inputs*/ changed - 4;
    ((Fl_Valuator*) win->cosuui.diffgroup->child(target))->value(((Fl_Valuator*) w)->value());

    switch (target)
    {
    case 2:
    case 6:
        win->update_ar_label();
        break;
    case 3:
    case 7:
        win->update_od_label();
        break;
    }
}

#pragma GCC diagnostic pop

#ifndef WIN32

static const char iconpaths[3][35] =
{
    "/usr/share/pixmaps/cosutrainer.png",
    "./cosutrainer.png",
    "../docs/cosutrainer.png",
};

static char *get_iconpath()
{
    for (int i = 0; i < 3; i++)
    {
        if (access(iconpaths[i], F_OK) == 0)
        {
            char *cpy = (char*) malloc(sizeof(iconpaths[i])); // may allocate larger memory than needed if it's short but whatever
            if (cpy == NULL)
            {
                printerr("Failed allocating!");
                return NULL;
            }
            strcpy(cpy, iconpaths[i]);
            return cpy;
        }
    }

    char *appi = getenv("APPDIR");
    if (appi != NULL)
    {
        const char suffix[] = "/usr/share/pixmaps/cosutrainer.png";
        char *path = (char*) malloc(strlen(appi) + sizeof(suffix));
        if (path == NULL)
        {
            printerr("Failed allocating!");
            return NULL;
        }
        strcpy(path, appi);
        strcat(path, suffix);
        return path;
    }

    return NULL;
}

Fl_RGB_Image *icon = NULL;

static void set_cosu_icon(Fl_Window *fwin)
{
    char *icp = get_iconpath();
    if (icp != NULL)
    {
        Fl_RGB_Image *newicon = new Fl_PNG_Image(icp);
        free(icp);
        fwin->icon(newicon);

        if (icon != NULL)
            delete icon;

        icon = newicon;
    }
}

#else

#include <FL/platform.h>
#include "app.rc"

static void set_cosu_icon(Fl_Window *fwin)
{
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    SendMessage(fl_xid(fwin), WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(fl_xid(fwin), WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
}

#endif

void CosuWindow::start()
{
    Fl::scheme("plastic");
    Fl_Image::RGB_scaling(FL_RGB_SCALING_BILINEAR);

    Fl_Window *window = cosuui.make_window();

    Fl_Image emptyimg(0,0,0);
    Fl_Image *img = NULL;

    cosuui.lock->hide();

    cosuui.rate->callback(rateradio_callb, (void*) this);
    cosuui.bpm->callback(bpmradio_callb, (void*) this);
    cosuui.reset->callback(resetbtn_callb, (void*) this);
    cosuui.convert->callback(convbtn_callb, (void*) this);
    cosuui.speedval->callback(speedval_callb, (void*) this);

    Fl_Widget *const *diffw = cosuui.diffgroup->array();
    for (int gi = cosuui.diffgroup->children() - 1; gi >= 0; gi--)
    {
        (*(diffw + gi))->callback(diffch_callb, (void*) this);
    }

    const char cut_tooltip[] = "Put only numbers for combo count, or \':\' for time.\n"
                               "Blanking the first box makes the converted map start from 0, and blanking the second makes it end at the end of an original map.\n"
                               "Note that combo count works properly only on the standard mode.\n"
                               "Example: 2385 for combo count, and 1:30 or 0:30 for time";

    cosuui.cutlabel->tooltip(cut_tooltip);
    cosuui.cutstart->tooltip(cut_tooltip);
    cosuui.cutend->tooltip(cut_tooltip);

    Fl::lock();

    window->show();
    window->make_current();

    set_cosu_icon(window);

    while (Fl::wait() > 0)
    {
        if (fr.info != NULL && fr.consumed == false)
        {
            std::lock_guard<std::recursive_mutex> lck(fr.mtx);
            struct mapinfo *info = fr.info;
            fr.consumed = true;
            Fl_Image *tempimg = NULL;
            char *bgpath = NULL;
            bool bgchanged = false;
            if (info->bgname != NULL && (fr.oldinfo == NULL || fr.oldinfo->bgname != NULL))
            {
                unsigned long oldfdlen = fr.oldinfo != NULL ? strrchr(fr.oldinfo->fullpath, PATHSEP) - fr.oldinfo->fullpath : 0;
                unsigned long fdlen = strrchr(info->fullpath, PATHSEP) - info->fullpath;

                if (fr.oldinfo == NULL || oldfdlen != fdlen || strncmp(fr.oldinfo->fullpath, info->fullpath, fdlen) != 0
                        || strcmp(fr.oldinfo->bgname, info->bgname) != 0)
                {
                    bgchanged = true;
                    char *sepa = strrchr(info->fullpath, PATHSEP);
                    unsigned long newlen = sepa - info->fullpath + 1 + strlen(info->bgname) + 1;
                    bgpath = (char*) malloc(newlen);
                    if (bgpath == NULL)
                    {
                        printerr("Error while allocating!");
                    }
                    else
                    {
                        memcpy(bgpath, info->fullpath, sepa - info->fullpath);
                        *(bgpath + (sepa - info->fullpath)) = PATHSEP;
                        memcpy(bgpath + (sepa - info->fullpath + 1), info->bgname, strlen(info->bgname) + 1);
                    }
                }
            }
            if ((info->bgname == NULL && (fr.oldinfo != NULL && fr.oldinfo->bgname != NULL))
                    || (info->bgname != NULL && (fr.oldinfo != NULL && fr.oldinfo->bgname == NULL)))
            {
                bgchanged = true;
            }
            if (bgpath != NULL)
            {
                if (endswith(bgpath, ".png"))
                {
                    tempimg = new Fl_PNG_Image(bgpath);
                }
                else if (endswith(bgpath, ".jpg") || endswith(bgpath, ".jpeg"))
                {
                    tempimg = new Fl_JPEG_Image(bgpath);
                }
                else
                {
                    tempimg = NULL;
                }

                if (tempimg != NULL)
                {
                    float newh = (370.0f / (float) (tempimg->w())) * (float) tempimg->h();
                    Fl_Image *res = ((Fl_RGB_Image*) tempimg)->copy(370, (int) newh);
                    delete tempimg;
                    tempimg = res;
                }
                free(bgpath);
            }
            if ((cosuui.hplock)->value() <= 0)
            {
                (cosuui.hpslider)->value(info->hp);
                (cosuui.hpinput)->value(info->hp);
            }
            if ((cosuui.cslock)->value() <= 0)
            {
                (cosuui.csslider)->value(info->cs);
                (cosuui.csinput)->value(info->cs);
            }
            if ((cosuui.odlock)->value() <= 0)
            {
                (cosuui.odslider)->value(info->od);
                (cosuui.odinput)->value(info->od);
            }
            if ((cosuui.arlock)->value() <= 0)
            {
                (cosuui.arslider)->value(info->ar);
                (cosuui.arinput)->value(info->ar);
            }

            if (info->mode == 2)
            {
                (cosuui.odslider)->deactivate();
                (cosuui.odinput)->deactivate();
                (cosuui.odlock)->deactivate();

                (cosuui.scale_od)->label("Scale OD");
                (cosuui.scale_od)->deactivate();
            }
            else
            {
                (cosuui.odslider)->activate();
                (cosuui.odinput)->activate();
                (cosuui.odlock)->activate();
                (cosuui.scale_od)->activate();
            }

            if (info->mode == 1 || info->mode == 3)
            {
                (cosuui.arslider)->deactivate();
                (cosuui.arinput)->deactivate();
                (cosuui.arlock)->deactivate();
                (cosuui.csslider)->deactivate();
                (cosuui.csinput)->deactivate();
                (cosuui.cslock)->deactivate();

                (cosuui.scale_ar)->label("Scale AR");
                (cosuui.scale_ar)->deactivate();
            }
            else
            {
                (cosuui.arslider)->activate();
                (cosuui.arinput)->activate();
                (cosuui.arlock)->activate();
                (cosuui.csslider)->activate();
                (cosuui.csinput)->activate();
                (cosuui.cslock)->activate();

                (cosuui.scale_ar)->activate();
            }

            if (info->mode == 3)
            {
                (cosuui.invert)->activate();
                (cosuui.nospinner)->label("No SV");
            }
            else
            {
                if ((cosuui.flipbox)->value() == 4)
                    (cosuui.flipbox)->value(0);

                (cosuui.invert)->deactivate();
                (cosuui.nospinner)->label("No Spinner");
            }

            cosuui.songtitlelabel->label(info->songname);
            cosuui.difflabel->label(info->diffname);
            if (bgpath != NULL && tempimg != NULL)
            {
                cosuui.infobox->image(tempimg);
                delete img;
                img = tempimg;
            }

            if ((bgchanged && tempimg == NULL) || info->bgname == NULL)
            {
                cosuui.infobox->image(emptyimg);
                delete img;
                img = NULL;
            }
            if (cosuui.bpm->value() >= 1 && cosuui.lock->value() <= 0)
            {
                cosuui.speedval->value(info->maxbpm);
            }
            if (cosuui.rate->value() >= 1)
            {
                update_rate_bpm();
            }
            update_ar_label();
            update_od_label();

            cosuui.infobox->redraw();
        }
    }
    if (img != NULL) delete img;
#ifndef WIN32
    if (icon != NULL) delete icon;
#endif
}
