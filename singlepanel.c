/*
 *
 *  postfish
 *    
 *      Copyright (C) 2002-2005 Monty
 *
 *  Postfish is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  Postfish is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with Postfish; see the file COPYING.  If not, write to the
 *  Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 */

#include "postfish.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "readout.h"
#include "multibar.h"
#include "mainpanel.h"
#include "subpanel.h"
#include "feedback.h"
#include "singlecomp.h"
#include "singlepanel.h"
#include "config.h"

typedef struct {
  GtkWidget *slider;
  GtkWidget *readouto;
  GtkWidget *readoutu;
  sig_atomic_t *vu;
  sig_atomic_t *vo;
} cbar;

typedef struct{
  Multibar *s;
  Readout *r;
  sig_atomic_t *v;
} callback_arg_rv;

typedef struct{
  Multibar *s;
  Readout *r0;
  Readout *r1;
  sig_atomic_t *v0;
  sig_atomic_t *v1;
} callback_arg_rv2;

typedef struct singlecomp_panel_state{
  subpanel_generic *panel;

  GtkWidget *o_peak;
  GtkWidget *o_rms;
  GtkWidget *o_knee;
  GtkWidget *u_peak;
  GtkWidget *u_rms;
  GtkWidget *u_knee;
  GtkWidget *b_peak;
  GtkWidget *b_rms;

  callback_arg_rv over_compand;
  callback_arg_rv under_compand;
  callback_arg_rv base_compand;

  callback_arg_rv over_lookahead;
  callback_arg_rv under_lookahead;

  callback_arg_rv2 over_timing;
  callback_arg_rv2 under_timing;
  callback_arg_rv2 base_timing;

  singlecomp_settings *ms;
  cbar bar;

} singlecomp_panel_state;

static singlecomp_panel_state *master_panel;
static singlecomp_panel_state *channel_panel[MAX_INPUT_CHANNELS];

static void singlepanel_state_to_config_helper(int bank,singlecomp_settings *s,int A){
  config_set_integer("singlecompand_active",bank,A,0,0,0,s->panel_active);
  config_set_integer("singlecompand_thresh",bank,A,0,0,0,s->u_thresh);
  config_set_integer("singlecompand_thresh",bank,A,0,0,1,s->o_thresh);

  config_set_integer("singlecompand_over_set",bank,A,0,0,0,s->o_mode);
  config_set_integer("singlecompand_over_set",bank,A,0,0,1,s->o_softknee);
  config_set_integer("singlecompand_over_set",bank,A,0,0,2,s->o_ratio);
  config_set_integer("singlecompand_over_set",bank,A,0,0,3,s->o_attack);
  config_set_integer("singlecompand_over_set",bank,A,0,0,4,s->o_decay);
  config_set_integer("singlecompand_over_set",bank,A,0,0,5,s->o_lookahead);

  config_set_integer("singlecompand_under_set",bank,A,0,0,0,s->u_mode);
  config_set_integer("singlecompand_under_set",bank,A,0,0,1,s->u_softknee);
  config_set_integer("singlecompand_under_set",bank,A,0,0,2,s->u_ratio);
  config_set_integer("singlecompand_under_set",bank,A,0,0,3,s->u_attack);
  config_set_integer("singlecompand_under_set",bank,A,0,0,4,s->u_decay);
  config_set_integer("singlecompand_under_set",bank,A,0,0,5,s->u_lookahead);

  config_set_integer("singlecompand_base_set",bank,A,0,0,0,s->b_mode);
  config_set_integer("singlecompand_base_set",bank,A,0,0,2,s->b_ratio);
  config_set_integer("singlecompand_base_set",bank,A,0,0,3,s->b_attack);
  config_set_integer("singlecompand_base_set",bank,A,0,0,4,s->b_decay);
}

void singlepanel_state_to_config(int bank){
  int i;
  singlepanel_state_to_config_helper(bank,&singlecomp_master_set,0);
  for(i=0;i<input_ch;i++)
    singlepanel_state_to_config_helper(bank,singlecomp_channel_set+i,i+1);
}

static void singlepanel_state_from_config_helper(int bank,singlecomp_settings *s,
						 singlecomp_panel_state *p,int A){

  config_get_sigat("singlecompand_active",bank,A,0,0,0,&s->panel_active);
  if(p)gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->panel->subpanel_activebutton[0]),s->panel_active);

  config_get_sigat("singlecompand_thresh",bank,A,0,0,0,&s->u_thresh);
  if(p)multibar_thumb_set(MULTIBAR(p->bar.slider),s->u_thresh,0);
  config_get_sigat("singlecompand_thresh",bank,A,0,0,1,&s->o_thresh);
  if(p)multibar_thumb_set(MULTIBAR(p->bar.slider),s->o_thresh,1);

  config_get_sigat("singlecompand_over_set",bank,A,0,0,0,&s->o_mode);
  if(p){
    if(s->o_mode)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->o_peak),1);
    else
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->o_rms),1);
  }

  config_get_sigat("singlecompand_over_set",bank,A,0,0,1,&s->o_softknee);
  if(p)gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->o_knee),s->o_softknee);
  config_get_sigat("singlecompand_over_set",bank,A,0,0,2,&s->o_ratio);
  if(p)multibar_thumb_set(p->over_compand.s,1000./s->o_ratio,0);
  config_get_sigat("singlecompand_over_set",bank,A,0,0,3,&s->o_attack);
  if(p)multibar_thumb_set(p->over_timing.s,s->o_attack*.1,0);
  config_get_sigat("singlecompand_over_set",bank,A,0,0,4,&s->o_decay);
  if(p)multibar_thumb_set(p->over_timing.s,s->o_decay*.1,1);
  config_get_sigat("singlecompand_over_set",bank,A,0,0,5,&s->o_lookahead);
  if(p)multibar_thumb_set(p->over_lookahead.s,s->o_lookahead*.1,0);

  config_get_sigat("singlecompand_under_set",bank,A,0,0,0,&s->u_mode);
  if(p){
    if(s->u_mode)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->u_peak),1);
    else
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->u_rms),1);
  }

  config_get_sigat("singlecompand_under_set",bank,A,0,0,1,&s->u_softknee);
  if(p)gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->u_knee),s->u_softknee);
  config_get_sigat("singlecompand_under_set",bank,A,0,0,2,&s->u_ratio);
  if(p)multibar_thumb_set(p->under_compand.s,1000./s->u_ratio,0);
  config_get_sigat("singlecompand_under_set",bank,A,0,0,3,&s->u_attack);
  if(p)multibar_thumb_set(p->under_timing.s,s->u_attack*.1,0);
  config_get_sigat("singlecompand_under_set",bank,A,0,0,4,&s->u_decay);
  if(p)multibar_thumb_set(p->under_timing.s,s->u_decay*.1,1);
  config_get_sigat("singlecompand_under_set",bank,A,0,0,5,&s->u_lookahead);
  if(p)multibar_thumb_set(p->under_lookahead.s,s->u_lookahead*.1,0);

  config_get_sigat("singlecompand_base_set",bank,A,0,0,0,&s->b_mode);
  if(p){
    if(s->b_mode)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->b_peak),1);
    else
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->b_rms),1);
  }
  config_get_sigat("singlecompand_base_set",bank,A,0,0,2,&s->b_ratio);
  if(p)multibar_thumb_set(p->base_compand.s,1000./s->b_ratio,0);
  config_get_sigat("singlecompand_base_set",bank,A,0,0,3,&s->b_attack);
  if(p)multibar_thumb_set(p->base_timing.s,s->b_attack*.1,0);
  config_get_sigat("singlecompand_base_set",bank,A,0,0,4,&s->b_decay);
  if(p)multibar_thumb_set(p->base_timing.s,s->b_decay*.1,1);

}

void singlepanel_state_from_config(int bank){
  int i;
  singlepanel_state_from_config_helper(bank,&singlecomp_master_set,master_panel,0);
  for(i=0;i<input_ch;i++)
    singlepanel_state_from_config_helper(bank,singlecomp_channel_set+i,channel_panel[i],i+1);
}

static void compand_change(GtkWidget *w,gpointer in){
  callback_arg_rv *ca=(callback_arg_rv *)in;
  char buffer[80];
  float val=1./multibar_get_value(MULTIBAR(w),0);

  if(val==1.){
    sprintf(buffer,"   off");
  }else if(val>=10){
    sprintf(buffer,"%4.1f:1",val);
  }else if(val>=1){
    sprintf(buffer,"%4.2f:1",val);
  }else if(val>.10001){
    sprintf(buffer,"1:%4.2f",1./val);
  }else{
    sprintf(buffer,"1:%4.1f",1./val);
  }

  readout_set(ca->r,buffer);
  
  *ca->v=rint(val*1000.);
}

static void timing_change(GtkWidget *w,gpointer in){
  callback_arg_rv2 *ca=(callback_arg_rv2 *)in;

  float attack=multibar_get_value(MULTIBAR(w),0);
  float decay=multibar_get_value(MULTIBAR(w),1);
  char buffer[80];

  if(attack<10){
    sprintf(buffer,"%4.2fms",attack);
  }else if(attack<100){
    sprintf(buffer,"%4.1fms",attack);
  }else if (attack<1000){
    sprintf(buffer,"%4.0fms",attack);
  }else if (attack<10000){
    sprintf(buffer,"%4.2fs",attack/1000.);
  }else{
    sprintf(buffer,"%4.1fs",attack/1000.);
  }
  readout_set(ca->r0,buffer);

  if(decay<10){
    sprintf(buffer,"%4.2fms",decay);
  }else if(decay<100){
    sprintf(buffer,"%4.1fms",decay);
  }else if (decay<1000){
    sprintf(buffer,"%4.0fms",decay);
  }else if (decay<10000){
    sprintf(buffer,"%4.2fs",decay/1000.);
  }else{
    sprintf(buffer,"%4.1fs",decay/1000.);
  }
  readout_set(ca->r1,buffer);

  *ca->v0=rint(attack*10.);
  *ca->v1=rint(decay*10.);
}

static void lookahead_change(GtkWidget *w,gpointer in){
  callback_arg_rv *ca=(callback_arg_rv *)in;
  char buffer[80];
  Readout *r=ca->r;
  float val=multibar_get_value(MULTIBAR(w),0);

  sprintf(buffer,"%3.0f%%",val);
  readout_set(r,buffer);
  
  *ca->v=rint(val*10.);
}

static void slider_change(GtkWidget *w,gpointer in){
  char buffer[80];
  cbar *b=(cbar *)in;
  int o,u;

  u=multibar_get_value(MULTIBAR(w),0);
  sprintf(buffer,"%+4ddB",u);
  readout_set(READOUT(b->readoutu),buffer);
  *b->vu=u;
  
  o=multibar_get_value(MULTIBAR(w),1);
  sprintf(buffer,"%+4ddB",o);
  readout_set(READOUT(b->readouto),buffer);
  *b->vo=o;
}

static void mode_rms(GtkButton *b,gpointer in){
  sig_atomic_t *var=(sig_atomic_t *)in;
  *var=0;
}

static void mode_peak(GtkButton *b,gpointer in){
  sig_atomic_t *var=(sig_atomic_t *)in;
  *var=1;
}

static void mode_knee(GtkToggleButton *b,gpointer in){
  int mode=gtk_toggle_button_get_active(b);
  sig_atomic_t *var=(sig_atomic_t *)in;
  *var=mode;
}

static singlecomp_panel_state *singlepanel_create_helper (postfish_mainpanel *mp,
							  subpanel_generic *panel,
							  singlecomp_settings *scset){

  char *labels[15]={"","130","120","110","100","90","80","70",
		    "60","50","40","30","20","10","0"};
  float levels[15]={-140,-130,-120,-110,-100,-90,-80,-70,-60,-50,-40,
		     -30,-20,-10,0};

  float compand_levels[9]={.1,.25,.5,.6667,1,1.5,2,4,10};
  char  *compand_labels[9]={"","4:1","2:1","1:1.5","1:1","1:1.5","1:2","1:4","1:10"};

  float timing_levels[6]={.5,1,10,100,1000,10000};
  char  *timing_labels[6]={"","1ms","10ms","100ms","1s","10s"};

  float per_levels[9]={0,12.5,25,37.5,50,62.5,75,87.5,100};
  char  *per_labels[9]={"0%","","25%","","50%","","75%","","100%"};

  singlecomp_panel_state *ps=calloc(1,sizeof(singlecomp_panel_state));
  ps->ms=scset;
  ps->panel=panel;

  GtkWidget *sliderframe=gtk_frame_new(NULL);
  GtkWidget *allbox=gtk_vbox_new(0,0);
  GtkWidget *slidertable=gtk_table_new(2,3,0);

  GtkWidget *overlabel=gtk_label_new("Over threshold compand ");
  GtkWidget *overtable=gtk_table_new(6,4,0);

  GtkWidget *underlabel=gtk_label_new("Under threshold compand ");
  GtkWidget *undertable=gtk_table_new(5,4,0);

  GtkWidget *baselabel=gtk_label_new("Global compand ");
  GtkWidget *basetable=gtk_table_new(3,4,0);

  gtk_widget_set_name(overlabel,"framelabel");
  gtk_widget_set_name(underlabel,"framelabel");
  gtk_widget_set_name(baselabel,"framelabel");

  gtk_misc_set_alignment(GTK_MISC(overlabel),0,.5);
  gtk_misc_set_alignment(GTK_MISC(underlabel),0,.5);
  gtk_misc_set_alignment(GTK_MISC(baselabel),0,.5);

    
  {
    GtkWidget *label1=gtk_label_new("compand threshold");
    GtkWidget *label2=gtk_label_new("under");
    GtkWidget *label3=gtk_label_new("over");

    gtk_misc_set_alignment(GTK_MISC(label1),.5,1.);
    gtk_misc_set_alignment(GTK_MISC(label2),.5,1.);
    gtk_misc_set_alignment(GTK_MISC(label3),.5,1.);
    gtk_widget_set_name(label2,"scalemarker");
    gtk_widget_set_name(label3,"scalemarker");

    gtk_table_attach(GTK_TABLE(slidertable),label2,0,1,0,1,GTK_FILL,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(slidertable),label1,1,2,0,1,GTK_FILL,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(slidertable),label3,2,3,0,1,GTK_FILL,GTK_FILL|GTK_EXPAND,2,0);
 
    gtk_container_add(GTK_CONTAINER(sliderframe),slidertable);

    //gtk_frame_set_shadow_type(GTK_FRAME(sliderframe),GTK_SHADOW_NONE);
    
    gtk_container_set_border_width(GTK_CONTAINER(sliderframe),4);
    gtk_container_set_border_width(GTK_CONTAINER(slidertable),10);

  }

  gtk_box_pack_start(GTK_BOX(panel->subpanel_box),allbox,0,0,0);
  gtk_box_pack_start(GTK_BOX(allbox),sliderframe,0,0,0);

  {
    GtkWidget *hs1=gtk_hseparator_new();
    GtkWidget *hs2=gtk_hseparator_new();

    //gtk_box_pack_start(GTK_BOX(allbox),hs3,0,0,0);
    gtk_box_pack_start(GTK_BOX(allbox),overtable,0,0,10);
    gtk_box_pack_start(GTK_BOX(allbox),hs1,0,0,0);
    gtk_box_pack_start(GTK_BOX(allbox),undertable,0,0,10);
    gtk_box_pack_start(GTK_BOX(allbox),hs2,0,0,0);
    gtk_box_pack_start(GTK_BOX(allbox),basetable,0,0,10);

    gtk_container_set_border_width(GTK_CONTAINER(overtable),5);
    gtk_container_set_border_width(GTK_CONTAINER(undertable),5);
    gtk_container_set_border_width(GTK_CONTAINER(basetable),5);

  }

  /* under compand: mode and knee */
  {
    GtkWidget *envelopebox=gtk_hbox_new(0,0);
    GtkWidget *rms_button=gtk_radio_button_new_with_label(NULL,"RMS");
    GtkWidget *peak_button=
      gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rms_button),
						  "peak");
    GtkWidget *knee_button=gtk_check_button_new_with_label("soft knee");

    gtk_box_pack_start(GTK_BOX(envelopebox),underlabel,0,0,0);
    gtk_box_pack_end(GTK_BOX(envelopebox),peak_button,0,0,5);
    gtk_box_pack_end(GTK_BOX(envelopebox),rms_button,0,0,5);
    gtk_box_pack_end(GTK_BOX(envelopebox),knee_button,0,0,5);

    g_signal_connect (G_OBJECT (knee_button), "clicked",
                      G_CALLBACK (mode_knee), &ps->ms->u_softknee);
    g_signal_connect (G_OBJECT (rms_button), "clicked",
                      G_CALLBACK (mode_rms), &ps->ms->u_mode);
    g_signal_connect (G_OBJECT (peak_button), "clicked",
                      G_CALLBACK (mode_peak), &ps->ms->u_mode);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rms_button),1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(knee_button),1);
    gtk_table_attach(GTK_TABLE(undertable),envelopebox,0,4,0,1,GTK_FILL,0,0,0);

    ps->u_rms=rms_button;
    ps->u_peak=peak_button;
    ps->u_knee=knee_button;
  }

  /* under compand: ratio */
  {

    GtkWidget *label=gtk_label_new("compand ratio:");
    GtkWidget *readout=readout_new("1.55:1");
    GtkWidget *slider=multibar_slider_new(9,compand_labels,compand_levels,1);
   
    ps->under_compand.s=MULTIBAR(slider);
    ps->under_compand.r=READOUT(readout);
    ps->under_compand.v=&ps->ms->u_ratio;

    multibar_callback(MULTIBAR(slider),compand_change,&ps->under_compand);
    multibar_thumb_set(MULTIBAR(slider),1.,0);

    gtk_misc_set_alignment(GTK_MISC(label),1.,.5);

    gtk_table_set_row_spacing(GTK_TABLE(undertable),0,4);
    gtk_table_attach(GTK_TABLE(undertable),label,0,1,1,2,GTK_FILL,0,2,0);
    gtk_table_attach(GTK_TABLE(undertable),slider,1,3,1,2,GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(undertable),readout,3,4,1,2,GTK_FILL,0,0,0);

  }

  /* under compand: timing */
  {

    GtkWidget *label=gtk_label_new("attack/decay:");
    GtkWidget *readout0=readout_new(" 100ms");
    GtkWidget *readout1=readout_new(" 100ms");
    GtkWidget *slider=multibar_slider_new(6,timing_labels,timing_levels,2);

    ps->under_timing.s=MULTIBAR(slider);
    ps->under_timing.r0=READOUT(readout0);
    ps->under_timing.r1=READOUT(readout1);
    ps->under_timing.v0=&ps->ms->u_attack;
    ps->under_timing.v1=&ps->ms->u_decay;
   
    multibar_callback(MULTIBAR(slider),timing_change,&ps->under_timing);
    multibar_thumb_set(MULTIBAR(slider),1,0);
    multibar_thumb_set(MULTIBAR(slider),100,1);

    gtk_misc_set_alignment(GTK_MISC(label),1,.5);

    gtk_table_set_row_spacing(GTK_TABLE(undertable),2,4);
    gtk_table_attach(GTK_TABLE(undertable),label,0,1,4,5,GTK_FILL,0,2,0);
    gtk_table_attach(GTK_TABLE(undertable),slider,1,2,4,5,GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(undertable),readout0,2,3,4,5,GTK_FILL,0,0,0);
    gtk_table_attach(GTK_TABLE(undertable),readout1,3,4,4,5,GTK_FILL,0,0,0);

  }

  /* under compand: lookahead */
  {

    GtkWidget *label=gtk_label_new("lookahead:");
    GtkWidget *readout=readout_new("100%");
    GtkWidget *slider=multibar_slider_new(9,per_labels,per_levels,1);
    
    ps->under_lookahead.s=MULTIBAR(slider);
    ps->under_lookahead.r=READOUT(readout);
    ps->under_lookahead.v=&ps->ms->u_lookahead;
    
    multibar_callback(MULTIBAR(slider),lookahead_change,&ps->under_lookahead);    
    multibar_thumb_set(MULTIBAR(slider),100.,0);
    multibar_thumb_increment(MULTIBAR(slider),1.,10.);

    gtk_misc_set_alignment(GTK_MISC(label),1,.5);
    
    gtk_table_set_row_spacing(GTK_TABLE(undertable),3,4);
    gtk_table_attach(GTK_TABLE(undertable),label,0,1,3,4,GTK_FILL,0,2,0);
    gtk_table_attach(GTK_TABLE(undertable),slider,1,3,3,4,GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(undertable),readout,3,4,3,4,GTK_FILL,0,0,0);
  }

  /* over compand: mode and knee */
  {
    GtkWidget *envelopebox=gtk_hbox_new(0,0);
    GtkWidget *rms_button=gtk_radio_button_new_with_label(NULL,"RMS");
    GtkWidget *peak_button=
      gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rms_button),
						  "peak");
    GtkWidget *knee_button=gtk_check_button_new_with_label("soft knee");

    gtk_box_pack_start(GTK_BOX(envelopebox),overlabel,0,0,0);
    gtk_box_pack_end(GTK_BOX(envelopebox),peak_button,0,0,5);
    gtk_box_pack_end(GTK_BOX(envelopebox),rms_button,0,0,5);
    gtk_box_pack_end(GTK_BOX(envelopebox),knee_button,0,0,5);

    g_signal_connect (G_OBJECT (knee_button), "clicked",
                      G_CALLBACK (mode_knee), &ps->ms->o_softknee);
    g_signal_connect (G_OBJECT (rms_button), "clicked",
                      G_CALLBACK (mode_rms), &ps->ms->o_mode);
    g_signal_connect (G_OBJECT (peak_button), "clicked",
                      G_CALLBACK (mode_peak), &ps->ms->o_mode);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rms_button),1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(knee_button),1);
    gtk_table_attach(GTK_TABLE(overtable),envelopebox,0,4,0,1,GTK_FILL,0,0,0);
    ps->o_rms=rms_button;
    ps->o_peak=peak_button;
    ps->o_knee=knee_button;
  }

  /* over compand: ratio */
  {

    GtkWidget *label=gtk_label_new("compand ratio:");
    GtkWidget *readout=readout_new("1.55:1");
    GtkWidget *slider=multibar_slider_new(9,compand_labels,compand_levels,1);

    ps->over_compand.s=MULTIBAR(slider);
    ps->over_compand.r=READOUT(readout);
    ps->over_compand.v=&ps->ms->o_ratio;

    multibar_callback(MULTIBAR(slider),compand_change,&ps->over_compand);
    multibar_thumb_set(MULTIBAR(slider),1.,0);

    gtk_misc_set_alignment(GTK_MISC(label),1.,.5);

    gtk_table_set_row_spacing(GTK_TABLE(overtable),0,4);
    gtk_table_attach(GTK_TABLE(overtable),label,0,1,1,2,GTK_FILL,0,2,0);
    gtk_table_attach(GTK_TABLE(overtable),slider,1,3,1,2,GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(overtable),readout,3,4,1,2,GTK_FILL,0,0,0);

  }

  /* over compand: timing */
  {

    GtkWidget *label=gtk_label_new("attack/decay:");
    GtkWidget *readout0=readout_new(" 100ms");
    GtkWidget *readout1=readout_new(" 100ms");
    GtkWidget *slider=multibar_slider_new(6,timing_labels,timing_levels,2);
   
    ps->over_timing.s=MULTIBAR(slider);
    ps->over_timing.r0=READOUT(readout0);
    ps->over_timing.r1=READOUT(readout1);
    ps->over_timing.v0=&ps->ms->o_attack;
    ps->over_timing.v1=&ps->ms->o_decay;
    
    multibar_callback(MULTIBAR(slider),timing_change,&ps->over_timing);
    multibar_thumb_set(MULTIBAR(slider),1,0);
    multibar_thumb_set(MULTIBAR(slider),100,1);

    gtk_misc_set_alignment(GTK_MISC(label),1,.5);

    gtk_table_set_row_spacing(GTK_TABLE(overtable),2,4);
    gtk_table_attach(GTK_TABLE(overtable),label,0,1,5,6,GTK_FILL,0,2,0);
    gtk_table_attach(GTK_TABLE(overtable),slider,1,2,5,6,GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(overtable),readout0,2,3,5,6,GTK_FILL,0,0,0);
    gtk_table_attach(GTK_TABLE(overtable),readout1,3,4,5,6,GTK_FILL,0,0,0);

  }

  /* over compand: lookahead */
  {

    GtkWidget *label=gtk_label_new("lookahead:");
    GtkWidget *readout=readout_new("100%");
    GtkWidget *slider=multibar_slider_new(9,per_labels,per_levels,1);
   
    ps->over_lookahead.s=MULTIBAR(slider);
    ps->over_lookahead.r=READOUT(readout);
    ps->over_lookahead.v=&ps->ms->o_lookahead;
    
    multibar_callback(MULTIBAR(slider),lookahead_change,&ps->over_lookahead);
    multibar_thumb_set(MULTIBAR(slider),100.,0);
    multibar_thumb_increment(MULTIBAR(slider),1.,10.);

    gtk_misc_set_alignment(GTK_MISC(label),1,.5);
    
    gtk_table_set_row_spacing(GTK_TABLE(overtable),3,4);
    gtk_table_attach(GTK_TABLE(overtable),label,0,1,3,4,GTK_FILL,0,2,0);
    gtk_table_attach(GTK_TABLE(overtable),slider,1,3,3,4,GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(overtable),readout,3,4,3,4,GTK_FILL,0,0,0);
  }


  /* base compand: mode */
  {
    GtkWidget *envelopebox=gtk_hbox_new(0,0);
    GtkWidget *rms_button=gtk_radio_button_new_with_label(NULL,"RMS");
    GtkWidget *peak_button=
      gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rms_button),
						  "peak");

    gtk_box_pack_start(GTK_BOX(envelopebox),baselabel,0,0,0);
    gtk_box_pack_end(GTK_BOX(envelopebox),peak_button,0,0,5);
    gtk_box_pack_end(GTK_BOX(envelopebox),rms_button,0,0,5);

    g_signal_connect (G_OBJECT (rms_button), "clicked",
                      G_CALLBACK (mode_rms), &ps->ms->b_mode);
    g_signal_connect (G_OBJECT (peak_button), "clicked",
                      G_CALLBACK (mode_peak), &ps->ms->b_mode);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rms_button),1);
    gtk_table_attach(GTK_TABLE(basetable),envelopebox,0,4,0,1,GTK_FILL,0,0,0);
    ps->b_rms=rms_button;
    ps->b_peak=peak_button;
  }

  /* base compand: ratio */
  {

    GtkWidget *label=gtk_label_new("compand ratio:");
    GtkWidget *readout=readout_new("1.55:1");
    GtkWidget *slider=multibar_slider_new(9,compand_labels,compand_levels,1);
   
    ps->base_compand.s=MULTIBAR(slider);
    ps->base_compand.r=READOUT(readout);
    ps->base_compand.v=&ps->ms->b_ratio;

    multibar_callback(MULTIBAR(slider),compand_change,&ps->base_compand);
    multibar_thumb_set(MULTIBAR(slider),1.,0);

    gtk_misc_set_alignment(GTK_MISC(label),1.,.5);

    gtk_table_set_row_spacing(GTK_TABLE(basetable),0,4);
    gtk_table_attach(GTK_TABLE(basetable),label,0,1,1,2,GTK_FILL,0,2,0);
    gtk_table_attach(GTK_TABLE(basetable),slider,1,3,1,2,GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(basetable),readout,3,4,1,2,GTK_FILL,0,0,0);

  }

  /* base compand: timing */
  {

    GtkWidget *label=gtk_label_new("attack/decay:");
    GtkWidget *readout0=readout_new(" 100ms");
    GtkWidget *readout1=readout_new(" 100ms");
    GtkWidget *slider=multibar_slider_new(6,timing_labels,timing_levels,2);

    ps->base_timing.s=MULTIBAR(slider);
    ps->base_timing.r0=READOUT(readout0);
    ps->base_timing.r1=READOUT(readout1);
    ps->base_timing.v0=&ps->ms->b_attack;
    ps->base_timing.v1=&ps->ms->b_decay;
   
    multibar_callback(MULTIBAR(slider),timing_change,&ps->base_timing);
    multibar_thumb_set(MULTIBAR(slider),1,0);
    multibar_thumb_set(MULTIBAR(slider),100,1);

    gtk_misc_set_alignment(GTK_MISC(label),1,.5);

    gtk_table_set_row_spacing(GTK_TABLE(basetable),2,4);
    gtk_table_attach(GTK_TABLE(basetable),label,0,1,4,5,GTK_FILL,0,2,0);
    gtk_table_attach(GTK_TABLE(basetable),slider,1,2,4,5,GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,2,0);
    gtk_table_attach(GTK_TABLE(basetable),readout0,2,3,4,5,GTK_FILL,0,0,0);
    gtk_table_attach(GTK_TABLE(basetable),readout1,3,4,4,5,GTK_FILL,0,0,0);

  }

  /* threshold control */

  {
    ps->bar.readoutu=readout_new("  +0");
    ps->bar.readouto=readout_new("  +0");
    ps->bar.slider=multibar_new(15,labels,levels,2,HI_DECAY|LO_DECAY|LO_ATTACK|PEAK_FOLLOW);
    ps->bar.vu=&ps->ms->u_thresh;
    ps->bar.vo=&ps->ms->o_thresh;

    multibar_callback(MULTIBAR(ps->bar.slider),slider_change,&ps->bar);
    multibar_thumb_set(MULTIBAR(ps->bar.slider),-140.,0);
    multibar_thumb_set(MULTIBAR(ps->bar.slider),0.,1);
    multibar_thumb_bounds(MULTIBAR(ps->bar.slider),-140,0);
    multibar_thumb_increment(MULTIBAR(ps->bar.slider),1.,10.);
    
    
    gtk_table_attach(GTK_TABLE(slidertable),ps->bar.readoutu,0,1,1,2,
		     0,0,0,0);
    gtk_table_attach(GTK_TABLE(slidertable),ps->bar.slider,1,2,1,2,
		     GTK_FILL|GTK_EXPAND,GTK_EXPAND,0,0);
    gtk_table_attach(GTK_TABLE(slidertable),ps->bar.readouto,2,3,1,2,
		     0,0,0,0);
  }
  
  subpanel_show_all_but_toplevel(panel);
  
  return ps;
}

static float *peakfeed=0;
static float *rmsfeed=0;

void singlepanel_feedback(int displayit){
  int j,k;
  if(!peakfeed){
    peakfeed=malloc(sizeof(*peakfeed)*max(input_ch,OUTPUT_CHANNELS));
    rmsfeed=malloc(sizeof(*rmsfeed)*max(input_ch,OUTPUT_CHANNELS));
  }
  
  if(pull_singlecomp_feedback_master(peakfeed,rmsfeed)==1)
    multibar_set(MULTIBAR(master_panel->bar.slider),rmsfeed,peakfeed,
		 OUTPUT_CHANNELS,(displayit && singlecomp_master_set.panel_visible));
  
  /* channel panels are a bit different; we want each in its native color */
  if(pull_singlecomp_feedback_channel(peakfeed,rmsfeed)==1){
    for(j=0;j<input_ch;j++){
      float rms[input_ch];
      float peak[input_ch];

      for(k=0;k<input_ch;k++){
	rms[k]=-150;
	peak[k]=-150;
      }
      
      rms[j]=rmsfeed[j];
      peak[j]=peakfeed[j];
      
      multibar_set(MULTIBAR(channel_panel[j]->bar.slider),rms,peak,
		   input_ch,(displayit && singlecomp_channel_set[j].panel_visible));
    }
  }
}

void singlepanel_reset(void){
  int j;
  multibar_reset(MULTIBAR(master_panel->bar.slider));
  for(j=0;j<input_ch;j++)
    multibar_reset(MULTIBAR(channel_panel[j]->bar.slider));
}


void singlepanel_create_master(postfish_mainpanel *mp,
			       GtkWidget *windowbutton,
			       GtkWidget *activebutton){
  
  char *shortcut[]={" s "};
  subpanel_generic *panel=subpanel_create(mp,windowbutton,&activebutton,
					  &singlecomp_master_set.panel_active,
					  &singlecomp_master_set.panel_visible,
					  "_Singleband Compand",shortcut,
					  0,1);
  master_panel=singlepanel_create_helper(mp,panel,&singlecomp_master_set);
}

void singlepanel_create_channel(postfish_mainpanel *mp,
				GtkWidget **windowbutton,
				GtkWidget **activebutton){
  int i;
  
  /* a panel for each channel */
  for(i=0;i<input_ch;i++){
    subpanel_generic *panel;
    char buffer[80];
    
    sprintf(buffer,"_Singleband Compand (channel %d)",i+1);
    panel=subpanel_create(mp,windowbutton[i],activebutton+i,
                          &singlecomp_channel_set[i].panel_active,
                          &singlecomp_channel_set[i].panel_visible,
                          buffer,NULL,
                          i,1);
    
    channel_panel[i]=singlepanel_create_helper(mp,panel,singlecomp_channel_set+i);
  }
}
