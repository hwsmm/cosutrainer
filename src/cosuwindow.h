#pragma once
#include "cosuui.h"
#include "freader.h"
#include "mapeditor.h"

class CosuWindow
{
private:
    CosuUI cosuui;
    Freader fr;
    double get_relative_speed();
    void update_rate_bpm();

    void update_ar_label();
    void update_od_label();
    static void update_progress(void *data, float progress);
    static void bpmradio_callb(Fl_Widget *w, void *data);
    static void rateradio_callb(Fl_Widget *w, void *data);
    static void resetbtn_callb(Fl_Widget *w, void *data);
    static void convbtn_callb(Fl_Widget *w, void *data);
    static void diffch_callb(Fl_Widget *w, void *data);
    static void speedval_callb(Fl_Widget *w, void *data);
public:
    CosuWindow();
    void start();
    void update_progress(float progress);
};
