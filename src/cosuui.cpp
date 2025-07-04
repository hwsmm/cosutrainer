// generated by Fast Light User Interface Designer (fluid) version 1.0311

#include "cosuui.h"

Fl_Menu_Item CosuUI::menu_flipbox[] =
{
    {"No flip", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
    {"X(flip)", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
    {"Y(flip)", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
    {"Transpose", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
    {"Invert (Full LN)", 0,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
    {0,0,0,0,0,0,0,0,0}
};
Fl_Menu_Item* CosuUI::no_flip = CosuUI::menu_flipbox + 0;
Fl_Menu_Item* CosuUI::x_flip = CosuUI::menu_flipbox + 1;
Fl_Menu_Item* CosuUI::y_flip = CosuUI::menu_flipbox + 2;
Fl_Menu_Item* CosuUI::transpose = CosuUI::menu_flipbox + 3;
Fl_Menu_Item* CosuUI::invert = CosuUI::menu_flipbox + 4;

Fl_Window* CosuUI::make_window()
{
    {
        window = new Fl_Window(390, 530, "cosu-trainer");
        window->user_data((void*)(this));
        {
            mainbox = new Fl_Group(10, 10, 370, 510);
            {
                infobox = new Fl_Box(10, 10, 370, 130);
                infobox->align(Fl_Align(FL_ALIGN_CLIP));
            } // Fl_Box* infobox
            {
                flipbox = new Fl_Choice(210, 220, 170, 25);
                flipbox->down_box(FL_BORDER_BOX);
                flipbox->menu(menu_flipbox);
            } // Fl_Choice* flipbox
            {
                scale_ar = new Fl_Check_Button(210, 260, 170, 15, "Scale AR");
                scale_ar->down_box(FL_DOWN_BOX);
                scale_ar->value(1);
            } // Fl_Check_Button* scale_ar
            {
                scale_od = new Fl_Check_Button(210, 290, 170, 15, "Scale OD");
                scale_od->down_box(FL_DOWN_BOX);
                scale_od->value(1);
            } // Fl_Check_Button* scale_od
            {
                pitch = new Fl_Check_Button(210, 320, 170, 15, "Adjust Pitch");
                pitch->down_box(FL_DOWN_BOX);
            } // Fl_Check_Button* pitch
            {
                nospinner = new Fl_Check_Button(210, 350, 170, 15, "No Spinner");
                nospinner->down_box(FL_DOWN_BOX);
            } // Fl_Check_Button* nospinner
            {
                rate = new Fl_Round_Button(210, 389, 85, 25, "Rate");
                rate->down_box(FL_ROUND_DOWN_BOX);
                rate->value(1);
            } // Fl_Round_Button* rate
            {
                bpm = new Fl_Round_Button(295, 389, 85, 25, "BPM");
                bpm->down_box(FL_ROUND_DOWN_BOX);
            } // Fl_Round_Button* bpm
            {
                speedval = new Fl_Spinner(210, 419, 80, 25);
                speedval->type(1);
                speedval->minimum(0.1);
                speedval->maximum(99999);
                speedval->step(0.1);
            } // Fl_Spinner* speedval
            {
                lock = new Fl_Check_Button(295, 419, 85, 25, "Lock");
                lock->down_box(FL_DOWN_BOX);
            } // Fl_Check_Button* lock
            {
                hplock = new Fl_Check_Button(20, 398, 15, 15);
                hplock->down_box(FL_DOWN_BOX);
            } // Fl_Check_Button* hplock
            {
                cslock = new Fl_Check_Button(70, 398, 15, 15);
                cslock->down_box(FL_DOWN_BOX);
            } // Fl_Check_Button* cslock
            {
                arlock = new Fl_Check_Button(120, 398, 15, 15);
                arlock->down_box(FL_DOWN_BOX);
            } // Fl_Check_Button* arlock
            {
                odlock = new Fl_Check_Button(170, 398, 15, 15);
                odlock->down_box(FL_DOWN_BOX);
            } // Fl_Check_Button* odlock
            {
                reset = new Fl_Button(10, 465, 100, 55, "Reset");
            } // Fl_Button* reset
            {
                convert = new Fl_Button(120, 465, 260, 55, "Convert now");
            } // Fl_Button* convert
            {
                songtitlelabel = new Fl_Box(10, 155, 370, 25, "Song Title");
                songtitlelabel->labelsize(25);
                songtitlelabel->align(Fl_Align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE));
            } // Fl_Box* songtitlelabel
            {
                difflabel = new Fl_Box(10, 185, 370, 25, "Difficulty");
                difflabel->align(Fl_Align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE));
            } // Fl_Box* difflabel
            {
                diffgroup = new Fl_Group(10, 220, 190, 175);
                {
                    hpslider = new Fl_Slider(10, 245, 40, 150, "HP");
                    hpslider->maximum(10);
                    hpslider->step(0.1);
                    hpslider->value(5);
                    hpslider->align(Fl_Align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE));
                } // Fl_Slider* hpslider
                {
                    csslider = new Fl_Slider(60, 245, 40, 150, "CS");
                    csslider->maximum(10);
                    csslider->step(0.1);
                    csslider->value(5);
                    csslider->align(Fl_Align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE));
                } // Fl_Slider* csslider
                {
                    arslider = new Fl_Slider(110, 245, 40, 150, "AR");
                    arslider->maximum(11);
                    arslider->step(0.1);
                    arslider->value(5);
                    arslider->align(Fl_Align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE));
                } // Fl_Slider* arslider
                {
                    odslider = new Fl_Slider(160, 245, 40, 150, "OD");
                    odslider->maximum(11);
                    odslider->step(0.1);
                    odslider->value(5);
                    odslider->align(Fl_Align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE));
                } // Fl_Slider* odslider
                {
                    hpinput = new Fl_Value_Input(10, 220, 40, 25);
                    hpinput->box(FL_THIN_DOWN_BOX);
                    hpinput->color((Fl_Color)23);
                    hpinput->maximum(10);
                    hpinput->step(0.1);
                    hpinput->value(5);
                } // Fl_Value_Input* hpinput
                {
                    csinput = new Fl_Value_Input(60, 220, 40, 25);
                    csinput->box(FL_THIN_DOWN_BOX);
                    csinput->color((Fl_Color)23);
                    csinput->maximum(10);
                    csinput->step(0.1);
                    csinput->value(5);
                } // Fl_Value_Input* csinput
                {
                    arinput = new Fl_Value_Input(110, 220, 40, 25);
                    arinput->box(FL_THIN_DOWN_BOX);
                    arinput->color((Fl_Color)23);
                    arinput->maximum(11);
                    arinput->step(0.1);
                    arinput->value(5);
                } // Fl_Value_Input* arinput
                {
                    odinput = new Fl_Value_Input(160, 220, 40, 25);
                    odinput->box(FL_THIN_DOWN_BOX);
                    odinput->color((Fl_Color)23);
                    odinput->maximum(11);
                    odinput->step(0.1);
                    odinput->value(5);
                } // Fl_Value_Input* odinput
                diffgroup->end();
            } // Fl_Group* diffgroup
            {
                ratebpm = new Fl_Box(295, 419, 85, 25, "0bpm");
                ratebpm->align(Fl_Align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE));
            } // Fl_Box* ratebpm
            {
                cutlabel = new Fl_Box(10, 419, 30, 25, "Cut");
                cutlabel->align(Fl_Align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE));
            } // Fl_Box* cutlabel
            {
                cutstart = new Fl_Input(41, 419, 70, 25);
            } // Fl_Input* cutstart
            {
                cutend = new Fl_Input(130, 419, 70, 25, "~ ");
            } // Fl_Input* cutend
            mainbox->end();
        } // Fl_Group* mainbox
        {
            progress = new Fl_Progress(10, 450, 370, 10);
            progress->selection_color((Fl_Color)181);
            progress->maximum(1);
            progress->minimum(0);
            progress->value(0);
        } // Fl_Progress* progress
        window->size_range(390, 530, 390, 530);
        window->end();
    } // Fl_Window* window
    return window;
}
