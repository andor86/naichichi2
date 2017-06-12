/**
 * @file   pass1.c
 * 
 * <JA>
 * @brief  �¡��ѥ����Վ������Ʊ���ӡ���õ��� *
 * ��Ū�ڹ�¤������Ѥ��ơ�������ħ�̥٥��Ȏ������Ф��ơ��Julius���裱�ѥ�
 * �Ǥ�����ե�����Ʊ���ӡ���õ����Ԥ��ޤ��. 
 *
 * ���ϥǡ������Τ����餫�������鎤��Ƥ�������ϡ�����Ƿ׻���� * �Ԥ��؎���get_back_trellis() ���ᥤ�󤫤�ƤЎ���ޤ��. ����饤��ǧ��
 * �Ύ���礎� realtime_1stpass.c ���顤�鎴������ե����ऴ�Ȥη׻����
 * ��λ�����Τ�������������ϤοʹԤˤ������Ƹ��̤˸ƤФ��ޤ��. 
 *
 * �ºݤθġ���ǧ���������󥹥��󥹤��Ȥν����� beam.c �˵��Ҥ�����Ƥ��ޤ��. 
 *
 * </JA>
 * 
 * <EN>
 * @brief  The first pass: frame-synchronous beam search
 *
 * These functions perform a frame-synchronous beam search using a static
 * lexicon tree, as the first pass of Julius/Julian.
 *
 * When the whole input is already obtained, get_back_trellis() simply
 * does all the processing of the 1st pass.  When performing online
 * real-time recognition with concurrent speech input, each function
 * will be called separately from realtime_1stpass.c according on the
 * basis of input processing.
 *
 * The core recognition processing functions for each recognition
 * process instances are written in beam.c.
 *
 * </EN>
 * 
 * @author Akinobu Lee
 * @date   Fri Oct 12 23:14:13 2007
 *
 * $Revision: 1.8 $
 * 
 */
/*
 * Copyright (c) 1991-2011 Kawahara Lab., Kyoto University
 * Copyright (c) 2000-2005 Shikano Lab., Nara Institute of Science and Technology
 * Copyright (c) 2005-2011 Julius project team, Nagoya Institute of Technology
 * All rights reserved
 */

#include <julius/julius.h>

/********************************************************************/
/* �裱�ѥ���¹Ԥ�����ᥤ��ؿ��                                    */
/* ���Ϥ�ѥ��ץ饤�������������礎� realtime_1stpass.c �򻲾ȤΤ��� */
/* main function to execute 1st pass                                */
/* the pipeline processing is not here: see realtime_1stpass.c      */
/********************************************************************/

/** 
 * <EN>
 * @brief  Process one input frame for all recognition process instance.
 *
 * This function proceeds the recognition for one frame.  All
 * recognition process instance will be processed synchronously.
 * The input frame for each instance is stored in mfcc->f, where mfcc
 * is the MFCC calculation instance assigned to each process instance.
 *
 * If an instance's mfcc->invalid is set to TRUE, its processing will
 * be skipped.
 *
 * When using GMM, GMM computation will also be executed here.
 * If GMM_VAD is defined, GMM-based voice detection will be performed
 * inside this function, by using a scheme of short-pause segmentation.
 *
 * This function also handles segmentation of recognition process.  A
 * segmentation will occur when end of speech is detected by level-based
 * sound detection or GMM-based / decoder-based VAD, or by request from
 * application.  When segmented, it stores current frame and return with
 * that status.
 *
 * The frame-wise callbacks will be executed inside this function,
 * when at least one valid recognition process instances exists.
 * 
 * </EN>
 * <JA>
 * @brief  ���Ƥ�ǧ���������󥹥��󥹽��������Վ������ʬ�ʤᤡ�
 *
 * ���Ƥ�ǧ���������󥹥��󥹤ˤĤ��ơ��䎤��դ��餡�Ƥ����FCC�׻����󥹥���
 * �� mfcc->f �򥫎����ȥե�����Ȥ��ƽ�������Վ������ʤᤡ� 
 *
 * �ʤ���mfcc->invalid �� TRUE �ȤʤäƤ�����������󥹥��󥹤ν����ϥ����å��
 * �������� 
 *
 * GMM�η׻��⤳���ǸƤӽФ������� GMM_VAD �ġ�����ϡ��GMM �ˤ莤�� * ȯ�ö�ֳ��ϡ���λ�θ��Ф������ǹԎ������� �ޤ���GMM�η׻�����̡��
 * ���������ǧ��������Υ��硼�ȥݡ����������ơ������Ƚ���ǥХ�������ɡ� * ������׵�ˤ莤��������ơ�������׵ᤵ�������ɤ�����Ƚ���Ԥ��. 
 *
 * �Վ������ñ�̤ǸƤӽФ������������Хå�����Ͽ�����Ƥ�������ϡ������餎�
 * �ƽФ���Ԥ�. 
 * </JA>
 * 
 * @param recog [in] engine instance
 * 
 * @return 0 on success, -1 on error, or 1 when an input segmentation
 * occured/requested inside this function.
 *
 * @callgraph
 * @callergraph
 * 
 */
int
decode_proceed(Recog *recog)
{
  MFCCCalc *mfcc;
  boolean break_flag;
  boolean break_decode;
  RecogProcess *p;
  boolean ok_p;
#ifdef GMM_VAD
  GMMCalc *gmm;
  boolean break_gmm;
#endif
  
  break_decode = FALSE;

  for(p = recog->process_list; p; p = p->next) {
    p->have_interim = FALSE;
  }
  for (mfcc = recog->mfcclist; mfcc; mfcc = mfcc->next) {
    mfcc->segmented = FALSE;
  }

  for(p = recog->process_list; p; p = p->next) {
    if (!p->live) continue;
    mfcc = p->am->mfcc;
    if (!mfcc->valid) {
      /* ���ΥՎ������ν����򥹥��å�� */
      /* skip processing the frame */
      continue;
    }

    /* mfcc-f �ΥՎ������ˤĤ���ǧ����͎�(�Վ������Ʊ���ӡ���õ�����ʤᎤ��*/
    /* proceed beam search for mfcc->f */
    if (mfcc->f == 0) {
      /* �ǽ�ΥՎ������� õ��������鎴���� */
      /* initial frame: initialize search process */
      if (get_back_trellis_init(mfcc->param, p) == FALSE) {
	jlog("ERROR: %02d %s: failed to initialize the 1st pass\n", p->config->id, p->config->name);
	return -1;
      }
    }
    if (mfcc->f > 0 || p->am->hmminfo->multipath) {
      /* 1�Վ������õ����ʤᤡ�*/
      /* proceed search for 1 frame */
      if (get_back_trellis_proceed(mfcc->f, mfcc->param, p, FALSE) == FALSE) {
	mfcc->segmented = TRUE;
	break_decode = TRUE;
      }
      if (p->config->successive.enabled) {
	if (detect_end_of_segment(p, mfcc->f - 1)) {
	  /* �������Ƚ�λ����: �裱�ѥ����������� */
	  mfcc->segmented = TRUE;
	  break_decode = TRUE;
	}
      }
    }
  }

  /* �������Ȥ��٤����ɤ����ǽ�Ū��Ƚ���Ԥ���
     �ǥ������١���VAD���������� spsegment �Ύ���硤ʣ�����󥹥��󥹴֤�� OR
     ��莤����ޤ����GMM�ʤ�ʣ����ब���������ϴ��֤�� AND ��莤����*/
  /* determine whether to segment at here
     If multiple segmenter exists, take their AND */
  break_flag = FALSE;
  if (break_decode
      ) {
    break_flag = TRUE;
  }

  if (break_flag) {
    /* õ�������ν�λ��ȯ�������ΤǤ�����ǧ���򽪤����� 
       �ǽ�ΥՎ�����फ���[f-1] ���ܤޤǤ�ǧ������������Ȥˤʤ��    */
    /* the recognition process tells us to stop recognition, so
       recognition should be terminated here.
       the recognized data are [0..f-1] */

    /* �ǽ��Վ�����ड�last_time �˥��å� */
    /* set the last frame to last_time */
    for (mfcc = recog->mfcclist; mfcc; mfcc = mfcc->next) {
      mfcc->last_time = mfcc->f - 1;
    }

    if (! recog->jconf->decodeopt.segment) {
      /* ���硼�ȥݡ����ʳ����ڎ�������硤�Ĥ�Υ���ץ���ǧ�������˼ΤƤ��*/
      /* drop rest inputs if segmented by error */
      for (mfcc = recog->mfcclist; mfcc; mfcc = mfcc->next) {
	mfcc->param->header.samplenum = mfcc->f;
	mfcc->param->samplenum = mfcc->f;
      }
    }

    return 1;
  }

  return 0;
}

#ifdef POWER_REJECT
boolean
power_reject(Recog *recog)
{
  MFCCCalc *mfcc;

  for (mfcc = recog->mfcclist; mfcc; mfcc = mfcc->next) {
    /* skip if not realtime and raw file processing */
    if (mfcc->avg_power == 0.0) continue;
    if (debug2_flag) jlog("STAT: power_reject: MFCC%02d: avg_power = %f\n", mfcc->id, mfcc->avg_power / mfcc->param->samplenum);
    if (mfcc->avg_power / mfcc->param->samplenum < recog->jconf->reject.powerthres) return TRUE;
  }
  return FALSE;
}
#endif

/** 
 * <EN>
 * @brief  End procedure of the first pass (when segmented)
 *
 * This function do things for ending the first pass and prepare for
 * the next recognition, when the input was segmented at the middle of
 * recognition by some reason.
 *
 * First, the best path at each recognition process instance will be parsed
 * and stored.  In case of recognition error or input rejection, the error
 * status will be set.
 *
 * Then, the last pause segment of the processed input will be cut and saved
 * to be processed at first in the recognition of the next or remaining input.
 * 
 * </EN>
 * <JA>
 * @brief  �¡��ѥ��ν�λ�����ʥ������Ȼ���
 * 
 * ���Ϥ����餫�λ�ͳ�ˤ�ä�����ǥ������Ȥ���������ˡ�¡��ѥ���ǧ���������� * ��λ���Ƽ���Ƴ����������ν�����Ԥ��. 
 *
 * �ޤ�����ǧ���������󥹥��󥹤��Ф��ơ�����ñ���������դ���¡��ѥ���
 * ǧ������̤Ȥ��Ƴ�Ǽ����� �ޤ���ǧ�����ԡ����ϴ��Ѥλ��ϥ��顼���ơ�������
 * ����������åȤ����
 * 
 * �����ơ������ǧ���ǡ����Υ������Ȥ�ǧ���򡤸��Ф���������������
 * ��֤���Ƴ����������ˡ���������������֤��ڤ��Ф��Ƥ���������Ƥ��. 
 * 
 * </JA>
 * 
 * @param recog [in] engine instance
 * 
 * @callgraph
 * @callergraph
 */
void
decode_end_segmented(Recog *recog)
{
  boolean ok_p;
  int mseclen;
  RecogProcess *p;
  int last_status;

  /* rejectshort �؎ġ���, ���Ϥ�û������Ф�����¡��ѥ�����̤���Ϥ��ʤ�� */
  /* suppress 1st pass output if -rejectshort and input shorter than specified */
  ok_p = TRUE;
  if (recog->jconf->reject.rejectshortlen > 0) {
    mseclen = (float)recog->mfcclist->last_time * (float)recog->jconf->input.period * (float)recog->jconf->input.frameshift / 10000.0;
    if (mseclen < recog->jconf->reject.rejectshortlen) {
      last_status = J_RESULT_STATUS_REJECT_SHORT;
      ok_p = FALSE;
    }
  }

#ifdef POWER_REJECT
  if (ok_p) {
    if (power_reject(recog)) {
      last_status = J_RESULT_STATUS_REJECT_POWER;
      ok_p = FALSE;
    }
  }
#endif

  if (ok_p) {
    for(p=recog->process_list;p;p=p->next) {
      if (!p->live) continue;
      finalize_1st_pass(p, p->am->mfcc->last_time);
    }
  } else {
    for(p=recog->process_list;p;p=p->next) {
      if (!p->live) continue;
      p->result.status = last_status;
    }
  }
  if (recog->jconf->decodeopt.segment) {
    finalize_segment(recog);
  }

  if (recog->gmm != NULL) {
    /* GMM �׻��ν�λ */
    gmm_end(recog);
  }
}

/** 
 * <EN>
 * @brief  End procedure of the first pass
 *
 * This function finish the first pass, when the input was fully
 * processed to the end.
 *
 * The best path at each recognition process instance will be parsed
 * and stored.  In case of recognition error or input rejection, the
 * error status will be set.
 *
 * </EN>
 * <JA>
 * @brief  �¡��ѥ��ν�λ����
 * 
 * ���Ϥ��Ǹ�ޤǽ���������ƽ�λ�����Ȥ��ˡ�¡��ѥ���ǧ���������� * ��λ�������� 
 *
 * ��ǧ���������󥹥��󥹤��Ф��ơ����λ����ǤΎ¡��ѥ��κ���ñ���� * ������Ǽ������ �ޤ���ǧ�����ԡ����ϴ��Ѥλ��ϥ��顼���ơ�������
 * ����������åȤ����
 * 
 * </JA>
 * 
 * @param recog [in] engine instance
 * 
 * @callgraph
 * @callergraph
 */
void
decode_end(Recog *recog)
{
  MFCCCalc *mfcc;
  int mseclen;
  boolean ok_p;
  RecogProcess *p;
  int last_status;

  for (mfcc = recog->mfcclist; mfcc; mfcc = mfcc->next) {
    mfcc->segmented = FALSE;
  }

  if (recog->gmm != NULL) {
    /* GMM �׻��ν�λ */
    gmm_end(recog);
  }

#ifdef GMM_VAD
  /* �⤷�Ȏ������������ʤ��ޤ����Ͻ�λ��ã�����Τʤ顤���Τޤޥ��顼��Ύ� */
  if (recog->jconf->decodeopt.segment) {
    if (recog->gmm) {
      if (recog->gc->after_trigger == FALSE) {
	for(p=recog->process_list;p;p=p->next) {
	  p->result.status = J_RESULT_STATUS_ONLY_SILENCE;	/* reject by decoding */
	}
	/* ���硼�ȥݡ����������ơ������Ύ�����
	   ���ϥѥ�᡼��ʬ��ʤɤκǽ�������Ԥʤ� */
	/* When short-pause segmentation enabled */
	finalize_segment(recog);
	return;
      }
    }
  }
#endif

  /* �裱�ѥ��κǸ�ΥՎ�������ǧ��������Ԥ�� */
  /* finalize 1st pass */
  for(p=recog->process_list;p;p=p->next) {
    if (!p->live) continue;
#ifdef SPSEGMENT_NAIST
    if (recog->jconf->decodeopt.segment) {
      if (p->pass1.after_trigger == FALSE) continue;
    }
#endif
    mfcc = p->am->mfcc;
    if (mfcc->f > 0) {
      get_back_trellis_end(mfcc->param, p);
    }
  }

  /* ��λ���� */
  for(p=recog->process_list;p;p=p->next) {
    if (!p->live) continue;

    ok_p = TRUE;

    /* check rejection by no input */
    if (ok_p) {
      mfcc = p->am->mfcc;
      /* ����Ĺ���ǎ�����η׻��˽�ʬ�Ǥʤ����硤����̵���Ȥ������ */
      /* if input is short for compute all the delta coeff., terminate here */
      if (mfcc->f == 0) {
//	jlog("STAT: no input frame\n");
	last_status = J_RESULT_STATUS_FAIL;
	ok_p = FALSE;
      }
    }

    /* check rejection by input length */
    if (ok_p) {
      if (recog->jconf->reject.rejectshortlen > 0) {
	mseclen = (float)mfcc->param->samplenum * (float)recog->jconf->input.period * (float)recog->jconf->input.frameshift / 10000.0;
	if (mseclen < recog->jconf->reject.rejectshortlen) {
	  last_status = J_RESULT_STATUS_REJECT_SHORT;
	  ok_p = FALSE;
	}
      }
    }

#ifdef POWER_REJECT
    /* check rejection by average power */
    if (ok_p) {
      if (power_reject(recog)) {
	last_status = J_RESULT_STATUS_REJECT_POWER;
	ok_p = FALSE;
      }
    }
#endif

#ifdef SPSEGMENT_NAIST
    /* check rejection non-triggered input segment */
    if (ok_p) {
      if (recog->jconf->decodeopt.segment) {
	if (p->pass1.after_trigger == FALSE) {
	  last_status = J_RESULT_STATUS_ONLY_SILENCE;	/* reject by decoding */
	  ok_p = FALSE;
	}
      }
    }
#endif

    if (ok_p) {
      /* valid input segment, finalize it */
      finalize_1st_pass(p, mfcc->param->samplenum);
    } else {
      /* invalid input segment */
      p->result.status = last_status;
    }
  }
  if (recog->jconf->decodeopt.segment) {
    /* ���硼�ȥݡ����������ơ������Ύ�����
       ���ϥѥ�᡼��ʬ��ʤɤκǽ�������Ԥʤ� */
    /* When short-pause segmentation enabled */
    finalize_segment(recog);
  }
}


/** 
 * <JA>
 * @brief  �Վ������Ʊ���ӡ���õ���ᥤ��ؿ��ʥХå������ѡ��
 *
 * Ϳ���鎤������ϥ٥��ȥ�����Ф����裱�ѥ��(�Վ������Ʊ���ӡ���õ������� * �Ԥ������Ύ���̤���Ϥ���� �ޤ����Վ��������Ϥ�������ü���裲�ѥ��
 * �Τ����ñ����ȥ�������¤�Τ˳�Ǽ����� 
 * 
 * ���δؿ������ϥ٥��Ȏ���󤬤��餫�������餡�Ƥ���������Ѥ��餡��� 
 * �裱�ѥ������Ϥ����󤷤Ƽ¹Ԥ����������饤��ǧ���ξ��硎�
 * ���δؿ����Ѥ��鎤������夡��ˤ��Υե�������ġ�������Ƥ����ƥ��ִؿ����
 * ľ�� realtime-1stpass.c �⤫��ƤЎ����� 
 * 
 * @param recog [in] ���󥸥󥤥󥹥���
 * </JA>
 * <EN>
 * @brief  Frame synchronous beam search: the main (for batch mode)
 *
 * This function perform the 1st recognition pass of frame-synchronous beam
 * search and output the result.  It also stores all the word ends in every
 * input frame to word trellis structure.
 *
 * This function will be called if the whole input vector is already given
 * to the end.  When online recognition, where the 1st pass will be
 * processed in parallel with input, this function will not be used.
 * In that case, functions defined in this file will be directly called
 * from functions in realtime-1stpass.c.
 * 
 * @param recog [in] engine instance
 * </EN>
 * @callgraph
 * @callergraph
 */
boolean
get_back_trellis(Recog *recog)
{
  boolean ok_p;
  MFCCCalc *mfcc;
  int rewind_frame;
  PROCESS_AM *am;
  boolean reprocess;

  /* initialize mfcc instances */
  for(mfcc=recog->mfcclist;mfcc;mfcc=mfcc->next) {
    /* mark all as valid, since all frames are fully prepared beforehand */
    if (mfcc->param->samplenum == 0) mfcc->valid = FALSE;
    else mfcc->valid = TRUE;
    /* set frame pointers to 0 */
    mfcc->f = 0;
  }

  recog->triggered = TRUE;

  while(1) {
    ok_p = TRUE;
    for (mfcc = recog->mfcclist; mfcc; mfcc = mfcc->next) {
      if (! mfcc->valid) continue;
      if (mfcc->f < mfcc->param->samplenum) {
	mfcc->valid = TRUE;
	ok_p = FALSE;
      } else {
	mfcc->valid = FALSE;
      }
    }
    if (ok_p) {
      /* ���٤Ƥ� MFCC ����������ã�����Τǥ����׽�Ύ� */
      /* all MFCC has been processed, end of loop  */
      break;
    }

    switch (decode_proceed(recog)) {
    case -1: /* error */
      return FALSE;
      break;
    case 0:			/* success */
      break;
    case 1:			/* segmented */
      /* õ������: ��������������Ϥ�� 0 ������t-2 �ޤ� */
      /* search terminated: processed input = [0..t-2] */
      /* ���λ����ǎ¡��ѥ���λ������*/
      /* end the 1st pass at this point */
      decode_end_segmented(recog);
      /* terminate 1st pass here */
      return TRUE;
    }


//    if (spsegment_need_restart(recog, &rewind_frame, &reprocess) == TRUE) {
//      /* do rewind for all mfcc here */
//      spsegment_restart_mfccs(recog, rewind_frame, reprocess);
//      /* reset outprob cache for all AM */
//      for(am=recog->amlist;am;am=am->next) {
//	outprob_prepare(&(am->hmmwrk), am->mfcc->param->samplenum);
//      }
//    }
    /* call frame-wise callback */
//    callback_exec(CALLBACK_EVENT_PASS1_FRAME, recog);

    /* 1�Վ������������ʤ���Τǥݥ��󥿤�ʤᤡ�*/
    /* proceed frame pointer */
    for (mfcc = recog->mfcclist; mfcc; mfcc = mfcc->next) {
      if (!mfcc->valid) continue;
      mfcc->f++;
    }

  }

  /* �ǽ��Վ�����������Ԥ���ǧ���η��̽��ϤȽ�λ������Ԥ�� */
  decode_end(recog);

  return TRUE;
}

/* end of file */