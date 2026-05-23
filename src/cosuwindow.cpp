#include "cosuwindow.h"
#include "packeditor.h"
#include "tools.h"
#include "cosuplatform.h"
#include <cstdio>
#include <cmath>
#include <thread>
#include <climits>
#include <cstdlib>
#include <FL/Fl_JPEG_Image.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/fl_ask.H>

using namespace std;

static double lod_table[4] = { 0.0, };
static double od_table[4] = { 10.0, };

CosuWindow::CosuWindow()
{
    for (int i = 0; i < 4; i++)
    {
        lod_table[i] = scale_od(0.0, 0.75, i);
        od_table[i] = scale_od(10.0, 1.5, i);
    }

    info = NULL;
    first = true;
    queue_reset = false;
    bpm_mode = GET_DEFAULT_BPM_MODE();
}

CosuWindow::~CosuWindow()
{
    free_mapinfo(info);
}

double CosuWindow::get_selected_bpm()
{
    if (info == NULL)
        return 1;

    return bpm_mode == max_bpm_mode ? info->maxbpm : main_bpm;
}

double CosuWindow::get_relative_speed()
{
    if (cosuui.rate->value() >= 1)
        return cosuui.speedval->value();
    else if (cosuui.bpm->value() >= 1)
        return cosuui.speedval->value() / get_selected_bpm();

    return 1;
}

char bpmstr[] = "xxxxxxxxxxxxxxbpm";

void CosuWindow::update_rate_bpm()
{
    double bpm = get_selected_bpm() * cosuui.speedval->value();
    snprintf(bpmstr, sizeof(bpmstr), "%.0lfbpm", bpm);

    cosuui.ratebpm->label(bpmstr);
}

char arstr[] = "Scale AR to xx.x";
char odstr[] = "Scale OD to xx.x";
const int offset = 12;

void CosuWindow::update_ar_label()
{
    if (info == NULL || info->mode == 1 || info->mode == 3)
        return;

    double scaled = scale_ar(cosuui.arslider->value(), get_relative_speed(), info->mode);
    CLAMP(scaled, -5, 11);

    snprintf(arstr + offset, sizeof(arstr) - offset, "%.1lf", scaled);
    cosuui.scale_ar->label(arstr);
}

void CosuWindow::update_od_label()
{
    if (info == NULL || info->mode == 2)
        return;

    double scaled = scale_od(cosuui.odslider->value(), get_relative_speed(), info->mode);
    CLAMP(scaled, lod_table[info->mode], od_table[info->mode]);

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
        win->cosuui.speedval->step(0.01);

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
        if (win->info != NULL)
        {
            win->cosuui.speedval->value(win->get_selected_bpm());
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
    win->cosuui.cv_mapset->clear();
    win->cosuui.cutstart->value("");
    win->cosuui.cutend->value("");
    win->queue_reset = true;
    Fl::awake();
}

void CosuWindow::update_progress(void *data, float progress)
{
    CosuWindow *win = (CosuWindow*) data;
    float current = win->cosuui.progress->value();
    if (progress > 0 && fabsf(progress - current) > 0.005)
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

    edit.mi = win->info;
    edit.hp = win->cosuui.hpslider->value();
    edit.cs = win->cosuui.csslider->value();
    edit.ar = win->cosuui.arslider->value();
    edit.od = win->cosuui.odslider->value();
    edit.scale_ar = win->cosuui.scale_ar->value() >= 1;
    edit.scale_od = win->cosuui.scale_od->value() >= 1;
    edit.cap_ar = edit.cap_od = false;
    edit.makezip = true;

    edit.speed = win->cosuui.speedval->value();
    edit.base_bpm = win->get_selected_bpm();
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
    case 5:
        edit.flip = fullrc;
        break;
    default:
        edit.flip = none;
        break;
    }

    edit.data = data;
    edit.progress_callback = update_progress;
    edit.cv_mapset = 0;

    int ret;
    if (win->cosuui.cv_mapset->value() >= 1)
    {
        edit.cv_mapset |= 1;

        if (win->cosuui.hplock->value() >= 1)
            edit.cv_mapset |= 1<<1;

        if (win->cosuui.cslock->value() >= 1)
            edit.cv_mapset |= 1<<2;

        if (win->cosuui.arlock->value() >= 1)
            edit.cv_mapset |= 1<<3;

        if (win->cosuui.odlock->value() >= 1)
            edit.cv_mapset |= 1<<4;

        ret = edit_beatmap_pack(&edit);
    }
    else
    {
        ret = edit_beatmap(&edit);
    }

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
unsigned char *icondata = NULL;

static void set_cosu_icon(Fl_Window *fwin)
{
    char *icp = get_iconpath();
    if (icp != NULL)
    {
        int x, y;
        icondata = stbi_load(icp, &x, &y, NULL, 3);
        free(icp);
        if (icondata == NULL)
            return;

        Fl_RGB_Image *newicon = new Fl_RGB_Image(icondata, x, y);
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

static enum BPM_MODE show_bpm_mode_dialog(enum BPM_MODE current_mode)
{
    enum BPM_MODE result = current_mode;

    Fl_Window dlg(280, 115, "Select BPM Mode");
    dlg.set_modal();

    Fl_Round_Button main_btn(20, 15, 240, 25, "Main BPM");
    Fl_Round_Button max_btn(20, 45, 240, 25, "Max BPM");
    Fl_Button ok(200, 80, 60, 25, "OK");

    if (current_mode == main_bpm_mode)
        main_btn.set();
    else
        max_btn.set();

    main_btn.callback([](Fl_Widget *, void *d) {
        ((Fl_Round_Button*)d)->clear();
    }, &max_btn);
    max_btn.callback([](Fl_Widget *, void *d) {
        ((Fl_Round_Button*)d)->clear();
    }, &main_btn);

    ok.callback([](Fl_Widget *w, void *) {
        w->window()->hide();
    });

    ok.take_focus();

    dlg.show();
    while (dlg.shown())
        Fl::wait();

    result = max_btn.value() >= 1 ? max_bpm_mode : main_bpm_mode;
    return result;
}

static char envstr[32767];
static CosuWindow *bpm_win = NULL;

int CosuWindow::handle_fltk_ev(int event)
{
    if (event == FL_SHORTCUT)
    {
        if (Fl::event_state(FL_CTRL))
        {
            if (Fl::event_key('f'))
            {
                const char *customfmt = getenv(CUSTOM_DIFF_VAR);
                const char *defaultfmt = DEFAULT_FMT;
                const char *usefmt = customfmt != NULL ? customfmt : defaultfmt;
                const char msg[] = "Define your custom difficulty name format:";

                const char *got = fl_input(msg, usefmt);
                if (got == NULL)
                    return 1;

                snprintf(envstr, sizeof(envstr), CUSTOM_DIFF_VAR "=%s", got);

                if (putenv(envstr) != 0)
                {
                    perror("putenv");
                    fl_alert("Failed setting an environment variable.\nCheck console for details.");
                }
                else
                {
                    fprintf(stderr, "New difficulty format is %s\n", getenv(CUSTOM_DIFF_VAR));
                }

                return 1;
            }
            else if (Fl::event_key('b') && bpm_win != NULL)
            {
                CosuWindow *win = bpm_win;
                bpm_win = NULL; // prevent multiple windows from being shown
                win->bpm_mode = show_bpm_mode_dialog(win->bpm_mode);

                if (win->bpm_mode == main_bpm_mode)
                {
                    if (win->main_bpm <= 0)
                        win->main_bpm = read_main_bpm(win->info);
                }
                else
                {
                    win->main_bpm = 0;
                }

                if (win->cosuui.rate->value() >= 1)
                {
                    win->update_rate_bpm();
                }
                else
                {
                    win->update_ar_label();
                    win->update_od_label();
                }

                bpm_win = win;

                return 1;
            }
        }
    }

    return 0;
}

void CosuWindow::start()
{
    bpm_win = this;
    Fl::scheme("plastic");
    Fl::visual(FL_RGB);
    Fl::add_handler(handle_fltk_ev);
    Fl_Image::RGB_scaling(FL_RGB_SCALING_BILINEAR);

    Fl_Window *window = cosuui.make_window();
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
    cosuui.mainbox->deactivate();

    const char cv_mapset_tooltip[] = "By default, converting an entire mapset makes converted maps retain their original difficulty values.\n"
                                     "You need to tick the lock checkboxes below the difficulty sliders to apply values to all converted maps.";

    cosuui.cv_mapset->tooltip(cv_mapset_tooltip);

    Fl::lock();

    window->show();
    window->make_current();

    set_cosu_icon(window);

    while (Fl::wait() > 0)
    {
        if (!queue_reset)
        {
            struct mapinfo *oldinfo = info;
            struct mapinfo *newinfo = fr.get_mapinfo();

            if (newinfo != NULL)
            {
                info = newinfo;
                free_mapinfo(oldinfo);

                if (bpm_mode == main_bpm_mode)
                    main_bpm = read_main_bpm(info);
                else
                    main_bpm = 0;

                cosuui.infobox->image((Fl_Image*)info->extra);
                delete img;
                img = (Fl_Image*)info->extra;
            }
            else
            {
                continue;
            }
        }
        else
        {
            queue_reset = false;

            if (info == NULL)
                continue;
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
            (cosuui.odslider)->minimum(lod_table[info->mode]);
            (cosuui.odinput)->minimum(lod_table[info->mode]);
            (cosuui.odslider)->maximum(od_table[info->mode]);
            (cosuui.odinput)->maximum(od_table[info->mode]);

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
            (cosuui.fullrc)->activate();
            (cosuui.nospinner)->label("No SV");
        }
        else
        {
            if ((cosuui.flipbox)->value() == 4 || (cosuui.flipbox)->value() == 5)
                (cosuui.flipbox)->value(0);

            (cosuui.invert)->deactivate();
            (cosuui.fullrc)->deactivate();
            (cosuui.nospinner)->label("No Spinner");
        }

        cosuui.songtitlelabel->label(info->songname);
        cosuui.difflabel->label(info->diffname);

        if (cosuui.bpm->value() >= 1 && cosuui.lock->value() <= 0)
        {
            cosuui.speedval->value(get_selected_bpm());
        }
        if (cosuui.rate->value() >= 1)
        {
            update_rate_bpm();
        }

        update_ar_label();
        update_od_label();
        cosuui.mainbox->activate();

        cosuui.infobox->redraw();
    }
    Fl::remove_handler(handle_fltk_ev);
    if (img != NULL) delete img;
#ifndef WIN32
    if (icon != NULL) delete icon;
    if (icondata != NULL) stbi_image_free(icondata);
#endif
}
