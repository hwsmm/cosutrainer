#pragma once
#include "cosuui.h"
#include "freader.h"
#include "mapeditor.h"

class CosuWindow
{
private:
    CosuUI cosuui;
    Freader fr;
    double get_selected_bpm();
    double get_relative_speed();
    void update_rate_bpm();

    struct mapinfo *info;
    bool first;
    bool queue_reset;
    enum BPM_MODE bpm_mode;
    double main_bpm;

    void update_ar_label();
    void update_od_label();
    void keep_sanity();

    static void update_progress(void *data, float progress);
    static void bpmradio_callb(Fl_Widget *w, void *data);
    static void rateradio_callb(Fl_Widget *w, void *data);
    static void resetbtn_callb(Fl_Widget *w, void *data);
    static void convbtn_callb(Fl_Widget *w, void *data);
    static void diffch_callb(Fl_Widget *w, void *data);
    static void speedval_callb(Fl_Widget *w, void *data);
    static void flipbox_callb(Fl_Widget *w, void *data);
    static int handle_fltk_ev(int event);
public:
    CosuWindow();
    ~CosuWindow();
    void start();
    void update_progress(float progress);
};
