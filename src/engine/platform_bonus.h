#pragma once 

fn_local null_term_string_t mouse_button_to_string(mouse_button_t button)
{
	null_term_string_t result = S("<invalid mouse button>");

	switch (button)
	{
		case Button_none:   result = S("<none>"); break;
		case Button_left:   result = S("left");   break;
		case Button_middle: result = S("middle"); break;
		case Button_right:  result = S("right");  break;
		case Button_x1:     result = S("x1");     break;
		case Button_x2:     result = S("x2");     break;
	}

	return result;
}

fn_local null_term_string_t keycode_to_string(keycode_t keycode)
{
	null_term_string_t result = S("<invalid keycode>");

	switch (keycode)
	{
		case Key_lbutton:        result = S("lbutton");        break;
		case Key_rbutton:        result = S("rbutton");        break;
		case Key_cancel:         result = S("cancel");         break;
		case Key_mbutton:        result = S("mbutton");        break;
		case Key_xbutton1:       result = S("xbutton1");       break;
		case Key_xbutton2:       result = S("xbutton2");       break;
		case Key_backspace:      result = S("backspace");      break;
		case Key_tab:            result = S("tab");            break;
		case Key_clear:          result = S("clear");          break;
		case Key_return:         result = S("return");         break;
		case Key_shift:          result = S("shift");          break;
		case Key_control:        result = S("control");        break;
		case Key_alt:            result = S("alt");            break;
		case Key_pause:          result = S("pause");          break;
		case Key_capslock:       result = S("capslock");       break;
		case Key_kana:           result = S("kana");           break; // NOTE: Could also be hangul, I have to pick one
		case Key_junja:          result = S("junja");          break;
		case Key_final:          result = S("final");          break; // NOTE: Could also be hanja, I have to pick one
		case Key_kanji:          result = S("kanji");          break;
		case Key_escape:         result = S("escape");         break;
		case Key_convert:        result = S("convert");        break;
		case Key_nonconvert:     result = S("nonconvert");     break;
		case Key_accept:         result = S("accept");         break;
		case Key_modechange:     result = S("modechange");     break;
		case Key_space:          result = S("space");          break;
		case Key_pageup:         result = S("pageup");         break;
		case Key_pagedown:       result = S("pagedown");       break;
		case Key_end:            result = S("end");            break;
		case Key_home:           result = S("home");           break;
		case Key_left:           result = S("left");           break;
		case Key_up:             result = S("up");             break;
		case Key_right:          result = S("right");          break;
		case Key_down:           result = S("down");           break;
		case Key_select:         result = S("select");         break;
		case Key_print:          result = S("print");          break;
		case Key_execute:        result = S("execute");        break;
		case Key_printscreen:    result = S("printscreen");    break;
		case Key_insert:         result = S("insert");         break;
		case Key_delete:         result = S("delete");         break;
		case Key_help:           result = S("help");           break;
		case Key_num_0:          result = S("num_0");          break;
		case Key_num_1:          result = S("num_1");          break;
		case Key_num_2:          result = S("num_2");          break;
		case Key_num_3:          result = S("num_3");          break;
		case Key_num_4:          result = S("num_4");          break;
		case Key_num_5:          result = S("num_5");          break;
		case Key_num_6:          result = S("num_6");          break;
		case Key_num_7:          result = S("num_7");          break;
		case Key_num_8:          result = S("num_8");          break;
		case Key_num_9:          result = S("num_9");          break;
		case Key_a:              result = S("a");              break;
		case Key_b:              result = S("b");              break;
		case Key_c:              result = S("c");              break;
		case Key_d:              result = S("d");              break;
		case Key_e:              result = S("e");              break;
		case Key_f:              result = S("f");              break;
		case Key_g:              result = S("g");              break;
		case Key_h:              result = S("h");              break;
		case Key_i:              result = S("i");              break;
		case Key_j:              result = S("j");              break;
		case Key_k:              result = S("k");              break;
		case Key_l:              result = S("l");              break;
		case Key_m:              result = S("m");              break;
		case Key_n:              result = S("n");              break;
		case Key_o:              result = S("o");              break;
		case Key_p:              result = S("p");              break;
		case Key_q:              result = S("q");              break;
		case Key_r:              result = S("r");              break;
		case Key_s:              result = S("s");              break;
		case Key_t:              result = S("t");              break;
		case Key_u:              result = S("u");              break;
		case Key_v:              result = S("v");              break;
		case Key_w:              result = S("w");              break;
		case Key_x:              result = S("x");              break;
		case Key_y:              result = S("y");              break;
		case Key_z:              result = S("z");              break;
		case Key_lsys:           result = S("lsys");           break;
		case Key_rsys:           result = S("rsys");           break;
		case Key_apps:           result = S("apps");           break;
		case Key_sleep:          result = S("sleep");          break;
		case Key_numpad0:        result = S("numpad0");        break;
		case Key_numpad1:        result = S("numpad1");        break;
		case Key_numpad2:        result = S("numpad2");        break;
		case Key_numpad3:        result = S("numpad3");        break;
		case Key_numpad4:        result = S("numpad4");        break;
		case Key_numpad5:        result = S("numpad5");        break;
		case Key_numpad6:        result = S("numpad6");        break;
		case Key_numpad7:        result = S("numpad7");        break;
		case Key_numpad8:        result = S("numpad8");        break;
		case Key_numpad9:        result = S("numpad9");        break;
		case Key_multiply:       result = S("multiply");       break;
		case Key_add:            result = S("add");            break;
		case Key_separator:      result = S("separator");      break;
		case Key_subtract:       result = S("subtract");       break;
		case Key_decimal:        result = S("decimal");        break;
		case Key_divide:         result = S("divide");         break;
		case Key_f1:             result = S("f1");             break;
		case Key_f2:             result = S("f2");             break;
		case Key_f3:             result = S("f3");             break;
		case Key_f4:             result = S("f4");             break;
		case Key_f5:             result = S("f5");             break;
		case Key_f6:             result = S("f6");             break;
		case Key_f7:             result = S("f7");             break;
		case Key_f8:             result = S("f8");             break;
		case Key_f9:             result = S("f9");             break;
		case Key_f10:            result = S("f10");            break;
		case Key_f11:            result = S("f11");            break;
		case Key_f12:            result = S("f12");            break;
		case Key_f13:            result = S("f13");            break;
		case Key_f14:            result = S("f14");            break;
		case Key_f15:            result = S("f15");            break;
		case Key_f16:            result = S("f16");            break;
		case Key_f17:            result = S("f17");            break;
		case Key_f18:            result = S("f18");            break;
		case Key_f19:            result = S("f19");            break;
		case Key_f20:            result = S("f20");            break;
		case Key_f21:            result = S("f21");            break;
		case Key_f22:            result = S("f22");            break;
		case Key_f23:            result = S("f23");            break;
		case Key_f24:            result = S("f24");            break;
		case Key_numlock:        result = S("numlock");        break;
		case Key_scroll:         result = S("scroll");         break;
		case Key_lshift:         result = S("lshift");         break;
		case Key_rshift:         result = S("rshift");         break;
		case Key_lcontrol:       result = S("lcontrol");       break;
		case Key_rcontrol:       result = S("rcontrol");       break;
		case Key_lalt:           result = S("lalt");           break;
		case Key_ralt:           result = S("ralt");           break;
		case Key_volumemute:     result = S("volumemute");     break;
		case Key_volumedown:     result = S("volumedown");     break;
		case Key_volumeup:       result = S("volumeup");       break;
		case Key_medianexttrack: result = S("medianexttrack"); break;
		case Key_mediaprevtrack: result = S("mediaprevtrack"); break;
		case Key_oem1:           result = S("oem1");           break;
		case Key_plus:           result = S("plus");           break;
		case Key_comma:          result = S("comma");          break;
		case Key_minus:          result = S("minus");          break;
		case Key_period:         result = S("period");         break;
		case Key_oem2:           result = S("oem2");           break;
		case Key_oem3:           result = S("oem3");           break;
		case Key_play:           result = S("play");           break;
		case Key_zoom:           result = S("zoom");           break;
		case Key_oemclear:       result = S("oemclear");       break;
	}

	return result;
}
