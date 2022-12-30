#include "cosuwindow.h"
#include "tools.h"
#include "mapeditor.h"
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <FL/Fl_JPEG_Image.H>
#include <FL/Fl_PNG_Image.H>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void CosuWindow::rateradio_callb(Fl_Widget *w, void *data)
{
   CosuWindow *win = (CosuWindow*) data;
   win->cosuui.lock->clear();
   if (((Fl_Round_Button*) w)->value() >= 1)
   {
      win->cosuui.bpm->clear();
      win->cosuui.lock->deactivate();
      win->cosuui.speedval->value(1);
      win->cosuui.speedval->step(0.1);
   }
}

void CosuWindow::bpmradio_callb(Fl_Widget *w, void *data)
{
   CosuWindow *win = (CosuWindow*) data;
   win->cosuui.lock->clear();
   if (((Fl_Round_Button*) w)->value() >= 1)
   {
      win->cosuui.rate->clear();
      win->cosuui.lock->activate();
      win->cosuui.speedval->step(1);
      if (win->fr.info != NULL)
      {
         win->cosuui.speedval->value(win->fr.info->maxbpm);
      }
      else
      {
         win->cosuui.speedval->value(1);
      }
   }
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
   win->fr.consumed = false;
   Fl::awake();
}

void CosuWindow::thr_convmap(CosuWindow *win)
{
   struct editdata edit;
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
   edit.nospinner = win->cosuui.nospinner->value() >= 1;

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
   default:
      edit.flip = none;
      break;
   }

   edit_beatmap(&edit, &(win->progress));

   win->done = true;
}

void CosuWindow::convbtn_callb(Fl_Widget *w, void *data)
{
   CosuWindow *win = (CosuWindow*) data;
   if (win->fr.info == NULL)
   {
      return;
   }

   win->fr.pause = true;
   win->cosuui.mainbox->deactivate();
   win->done = false;
   Fl::awake();

   win->cur = new std::thread(CosuWindow::thr_convmap, win);
}

void CosuWindow::diffch_callb(Fl_Widget *w, void *data)
{
   CosuWindow *win = (CosuWindow*) data;
   int changed = win->cosuui.diffgroup->find(w);
   if (changed >= 8) return;

   int target = changed < 4 /*0,1,2,3 sliders*/ ? changed + 4 : /*4,5,6,7 inputs*/ changed - 4;
   ((Fl_Valuator*) win->cosuui.diffgroup->child(target))->value(((Fl_Valuator*) w)->value());
}

#pragma GCC diagnostic pop

void CosuWindow::start()
{
   Fl::scheme("plastic");
   Fl_Window *window = cosuui.make_window();

   cosuui.rate->callback(rateradio_callb, (void*) this);
   cosuui.bpm->callback(bpmradio_callb, (void*) this);
   cosuui.reset->callback(resetbtn_callb, (void*) this);
   cosuui.convert->callback(convbtn_callb, (void*) this);

   Fl_Widget *const *diffw = cosuui.diffgroup->array();
   for (int gi = cosuui.diffgroup->children() - 1; gi >= 0; gi--)
   {
      (*(diffw + gi))->callback(diffch_callb, (void*) this);
   }

   window->show();

   Fl_Image emptyimg(0,0,0);
   Fl_Image *img = NULL;

   done = true;

   while (Fl::wait() > 0)
   {
      if (!done)
      {
         while (1)
         {
            cosuui.progress->value(progress);
            Fl::flush();
            if (done)
            {
               progress = 0;
               cosuui.progress->value(progress);
               cosuui.mainbox->activate();
               fr.pause = false;
               cur->join();
               delete cur;
               break;
            }
            usleep(100000); // using this instead of Fl::wait since wait just waits for long time for no reason, this program should not look for any events at this moment anyway
         }
      }
      if (fr.info != NULL && fr.consumed == false)
      {
         struct mapinfo *info = fr.info;
         fr.consumed = true;
         Fl_Image *tempimg = NULL;
         char *bgpath = NULL;
         bool bgchanged = false;
         if (info->bgname != NULL && (fr.oldinfo == NULL || fr.oldinfo->bgname != NULL))
         {
            unsigned long oldfdlen = fr.oldinfo != NULL ? strrchr(fr.oldinfo->fullpath, '/') - fr.oldinfo->fullpath : 0;
            unsigned long fdlen = strrchr(info->fullpath, '/') - info->fullpath;

            if (fr.oldinfo == NULL || oldfdlen != fdlen || strncmp(fr.oldinfo->fullpath, info->fullpath, fdlen) != 0
                  || strcmp(fr.oldinfo->bgname, info->bgname) != 0)
            {
               bgchanged = true;
               char *sepa = strrchr(info->fullpath, '/');
               unsigned long newlen = sepa - info->fullpath + 1 + strlen(info->bgname) + 1;
               bgpath = (char*) malloc(newlen);
               if (bgpath == NULL)
               {
                  printerr("Error while allocating!");
               }
               else
               {
                  memcpy(bgpath, info->fullpath, sepa - info->fullpath);
                  *(bgpath + (sepa - info->fullpath)) = '/';
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
               Fl_Image *res = ((Fl_RGB_Image*) tempimg)->copy(370, 208);
               delete tempimg;
               tempimg = res;
            }
            free(bgpath);
         }
         Fl::lock();
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
         cosuui.infobox->redraw();
         Fl::unlock();
      }
   }
   if (img != NULL) delete img;
}
