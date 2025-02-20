__asm("jmp kmain");

#define VIDEO_BUF_PTR (0xb8000)

/* ----------- Defines ----------- */

#define MAX(a, b) (a)>(b) ? (a) : (b)

#define PIC1_PORT (0x20)

#define IDT_TYPE_INTR (0x0E)
#define IDT_TYPE_TRAP (0x0F)

// Селектор секции кода, установленный загрузчиком ОС
#define GDT_CS (0x8)

#define CURSOR_PORT (0x3D4)
#define VIDEO_WIDTH (80) // Ширина текстового экрана

#define LINE_MAX_SIZE 40
#define PARSER_CAP    25
#define ALPH_SIZE     26
#define ASCCI_SIZE    128

#define ENTER_CODE      0x1c
#define CAPS_CODE       0x3a
#define BACKSPACE_CODE  0x0e

#define DEFAULT_COLOR 0x07

#define STD_ALGO 1
#define BM_ALGO  0

#define WINDOW_COLS 80
#define WINDOW_ROWS 25

/* ----------- Types ----------- */
typedef unsigned int    size_t;
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;


/* ----------- STD functions ----------- */


void* memset(void* dst, int ch, size_t sz) {
  char* ptr = (char*)dst;
  while(sz--) *ptr++ = ch;
  return dst;
}

size_t strlen(char* dst) {
  char* ptr = dst;
  size_t count = 0;
  while(*ptr++) count++;
  return count;
}

int strncmp(char* a, char* b, size_t n) {
  while(*a && *b && n && (*a == *b)) {
    a++; b++; n--;
  }

  if(n == 0) return 0;
  else return *(uint8_t*)a - *(uint8_t*)b;
}

void strncpy(char* a, char* b, size_t n) {
  size_t i = 0;
  while(i++ < n && *b) {
    *a++ = *b++;
  }
}

/* ----------- Interrupts ----------- */

// Структура описывает данные об обработчике прерывания
struct idt_entry {
  unsigned short base_lo; // Младшие биты адреса обработчика
  unsigned short segm_sel; // Селектор сегмента кода
  unsigned char always0; // Этот байт всегда 0
  unsigned char flags; // Флаги тип. Флаги: P, DPL, Типы - это константы - IDT_TYPE...
  unsigned short base_hi; // Старшие биты адреса обработчика
} __attribute__((packed)); // Выравнивание запрещено


// Структура, адрес которой передается как аргумент команды lidt
struct idt_ptr {
  unsigned short limit;
  unsigned int base;
} __attribute__((packed)); // Выравнивание запрещено

struct idt_entry g_idt[256]; // Реальная таблица IDT
struct idt_ptr g_idtp; // Описатель таблицы для команды lidt

// Пустой обработчик прерываний. Другие обработчики могут быть реализованы по этому шаблону
void default_intr_handler() {
  asm("pusha");
  // ... (реализация обработки)
  asm("popa; leave; iret");
}


typedef void (*intr_handler)();

void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr) { 
  unsigned int hndlr_addr = (unsigned int) hndlr;
  g_idt[num].base_lo = (unsigned short) (hndlr_addr & 0xFFFF);
  g_idt[num].segm_sel = segm_sel;
  g_idt[num].always0 = 0;
  g_idt[num].flags = flags;
  g_idt[num].base_hi = (unsigned short) (hndlr_addr >> 16);
}

// Функция инициализации системы прерываний: заполнение массива с адресами обработчиков
void intr_init() {
  int i;
  int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
  for(i = 0; i < idt_count; i++)
    intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
}

void intr_start() {
  int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
  g_idtp.base = (unsigned int) (&g_idt[0]);
  g_idtp.limit = (sizeof (struct idt_entry) * idt_count) - 1;
  asm("lidt %0" : : "m" (g_idtp) );
}

void intr_enable() {
  asm("sti");
}

void intr_disable() {
  asm("cli");
}

static inline unsigned char inb (unsigned short port) {
  unsigned char data;
  asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
  return data;
}

static inline void outb (unsigned short port, unsigned char data) {
  asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

static inline void outw (uint16_t port, uint16_t data) {
  asm volatile ("outw %w0, %w1" : : "a" (data), "Nd" (port));
}

void on_key(char scan_code);

void keyb_process_keys() {
  // Проверка что буфер PS/2 клавиатуры не пуст (младший бит присутствует)
  if (inb(0x64) & 0x01) {
    unsigned char scan_code;
    unsigned char state;
    scan_code = inb(0x60); // Считывание символа с PS/2 клавиатуры
    if (scan_code < 128) // Скан-коды выше 128 - это отпускание клавиши
        on_key(scan_code);
    }
}

void keyb_handler() {
  asm("pusha");
  // Обработка поступивших данных
  keyb_process_keys();
  // Отправка контроллеру 8259 нотификации о том, что прерывание обработано
  outb(PIC1_PORT, 0x20);
  asm("popa; leave; iret");
}

void keyb_init() {
  // Регистрация обработчика прерывания
  intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler);
  // segm_sel=0x8, P=1, DPL=0, Type=Intr
  // Разрешение только прерываний клавиатуры от контроллера 8259
  outb(PIC1_PORT + 1, 0xFF ^ 0x02); // 0xFF - все прерывания, 0x02 - бит IRQ1 (клавиатура).
  // Разрешены будут только прерывания, чьи биты установлены в 0
}

/* ----------- Hashes ----------- */

// http://www.techhelpmanual.com/57-keyboard_scan_codes.html
char scan_codes[128] = {
  0, 0,
  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 0, 0, '\\',
  'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '
};

char upper_hash[128];
char lower_hash[128];

/* ----------- Utils ----------- */


typedef struct user_state_t_ {
  size_t pos_x_;
  size_t pos_y_;

  char curr_line_[LINE_MAX_SIZE];

  uint8_t start_value_; // bm | std
  
  uint8_t flag_caps_;
} user_state_t;

user_state_t state = {
  .pos_x_ = 0,
  .pos_y_ = 0,
  .curr_line_ = {0},
  .flag_caps_ = 0
};

void handle_line();

void out_str(int color, const char* ptr, unsigned int strnum) {
  unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
  video_buf += 80*2 * strnum;
  while (*ptr) {
    video_buf[0] = (unsigned char) *ptr; // Символ (код)
    video_buf[1] = color; // Цвет символа и фона
    video_buf += 2;
    ptr++;
  }
}



// Функция переводит курсор на строку strnum (0 – самая верхняя) в позицию 
// pos на этой строке (0 – самое левое положение).
void cursor_moveto(unsigned int strnum, unsigned int pos) {
  unsigned short new_pos = (strnum * VIDEO_WIDTH) + pos;
  outb(CURSOR_PORT, 0x0F);
  outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
  outb(CURSOR_PORT, 0x0E);
  outb(CURSOR_PORT + 1, (unsigned char)( (new_pos >> 8) & 0xFF));
}
 
void new_line() {
  state.pos_x_ = 0;
  state.pos_y_++;
  memset(state.curr_line_, 0, LINE_MAX_SIZE);
}

void out_str_new_line(const char* str) {
  out_str(DEFAULT_COLOR, str, state.pos_y_);
  new_line();

  cursor_moveto(state.pos_y_, state.pos_x_);
}

void out_str_default(const char* str) {
  out_str(DEFAULT_COLOR, str, state.pos_y_);
  
  cursor_moveto(state.pos_y_, state.pos_x_);
}

void on_key(char scan_code) {
  if(state.pos_x_ >= LINE_MAX_SIZE) {
    return;
    // new_line();
  }
  
  if(scan_code == ENTER_CODE) {
    handle_line();
  }

  if(scan_code == CAPS_CODE) {
    state.flag_caps_++;
    state.flag_caps_ %= 2;
    return;
  }

  if(scan_code == BACKSPACE_CODE) {
    if(state.pos_x_ > 1) {
      state.curr_line_[(state.pos_x_--)-1] = ' ';
      state.curr_line_[state.pos_x_+1] = '\0';
    } else if(state.pos_x_ == 1) {
      state.curr_line_[(state.pos_x_--)-1] = ' ';
    } else {
      state.curr_line_[state.pos_x_] = '\0';
    }
  }

  if(scan_codes[scan_code]) {
    state.curr_line_[state.pos_x_++] = (state.flag_caps_) 
      ? upper_hash[scan_codes[scan_code]] 
      : scan_codes[scan_code];
  }
  
  out_str_default(state.curr_line_);
}

void init_upper() {
  for(int i=0; i<128; i++) upper_hash[i] = i;
  for(int i=97; i<=122; i++) upper_hash[i] = i-32;
  for(int i=65; i<=90; i++) upper_hash[i] = i;
}


void init_lower() {
  for(int i=0; i<128; i++) lower_hash[i] = i;
  for(int i=65; i<=90; i++) lower_hash[i] = i+32;
  for(int i=97; i<=122; i++) lower_hash[i] = i;
}

void init_codes_table() {
  for(int i=58; i<ASCCI_SIZE; i++) scan_codes[i] = 0;
}

size_t to_string(int num, char* dst) {
  if(num == 0) {
    *dst = '0';
    return 1;
  }

  char stack[LINE_MAX_SIZE];
  int sz = 0;
  while(num) {
    stack[sz++] = (num%10)+'0';
    num /= 10;
  }

  size_t tmp = sz;
  
  sz--;
  while(sz >= 0) *dst++ = stack[sz--];

  return tmp;
}

/* ----------- Main Logic ----------- */

typedef struct parser_t_ {
  char buf_[PARSER_CAP][LINE_MAX_SIZE];
  size_t sz_;
} parser_t;

typedef struct template_t_ {
  char buf_[LINE_MAX_SIZE];
  size_t type_;
  size_t sz_;
  int table_[ASCCI_SIZE];
  uint8_t flag_loaded_;
} template_t;

parser_t parser = {
  .buf_ = {0},
  .sz_ = 0
};

template_t template = {
  .buf_ = {0},
  .type_ = 0,
  .sz_ = 0,
  .table_ = {0},
  .flag_loaded_ = 0
};

void clear_parser() {
  memset(parser.buf_, 0, sizeof(char)*PARSER_CAP*LINE_MAX_SIZE);
  parser.sz_ = 0;
}

void print_parser() {
  for(int i=0; i<parser.sz_; i++) {
    out_str_new_line((const char*)parser.buf_[i]);
  }
}

void clear_screen() {
  char clear_buf[WINDOW_COLS+1];
  for(int i=0; i<WINDOW_COLS; i++) clear_buf[i] = ' ';
  clear_buf[WINDOW_COLS] = '\0';

  memset(state.curr_line_, 0, LINE_MAX_SIZE);

  for(int i=0; i<WINDOW_ROWS; i++) {
    out_str(DEFAULT_COLOR, clear_buf, i);
  }
}

void parse_curr_line() {
  clear_parser();
  
  char* ptr = state.curr_line_;
  size_t curr_len = 0;
  while(*ptr) {
    if(*ptr == ' ') {
      parser.sz_++;
      curr_len = 0;
    } else {
      parser.buf_[parser.sz_][curr_len++] = *ptr;
    }
    ptr++;
  }
  parser.sz_++;

}


void info() {
  out_str_new_line("---------------------------");
  out_str_new_line("Welcome to StringOs");
  out_str_new_line("Author: Ibadulaev Ivan");
  out_str_new_line("Study Group: 5151003/30002");
  out_str_new_line("---------------------------");
}

void upcase() {
  new_line();
  char buf[LINE_MAX_SIZE];
  
  for(int i=1; i<parser.sz_; i++) {
    memset(buf, 0, LINE_MAX_SIZE);
    char* ptr = parser.buf_[i];
    char* buf_ptr = buf;

    while(*ptr) *buf_ptr++ = upper_hash[*ptr++];

    out_str_new_line(buf);
  }
}

void downcase() {
  new_line();

  char buf[LINE_MAX_SIZE];
  
  for(int i=1; i<parser.sz_; i++) {
    memset(buf, 0, LINE_MAX_SIZE);
    char* ptr = parser.buf_[i];
    char* buf_ptr = buf;

    while(*ptr) *buf_ptr++ = lower_hash[*ptr++];

    out_str_new_line(buf);
  }
}

void titlize() {
  new_line();

  char buf[LINE_MAX_SIZE*2];
  char* ptr = buf;
  for(int i=1; i<parser.sz_; i++) {
    size_t s_len = strlen(parser.buf_[i]);
    strncpy(ptr, parser.buf_[i], s_len);

    *ptr = upper_hash[*ptr];

    ptr += s_len;
    *ptr = ' ';
    ptr++;
  }

  out_str_new_line(buf);
}

void print_template() {
  char buf[LINE_MAX_SIZE*2] = "Template  '";
  char* ptr = buf+11;
  
  size_t t_len = strlen(template.buf_);
  strncpy(ptr, template.buf_, t_len);
  ptr += t_len;
  *ptr++ = '\'';
  
  strncpy(ptr, " loaded.", 8);

  out_str_new_line(buf);
 
  if(template.type_ == BM_ALGO) {
    memset(buf, 0, LINE_MAX_SIZE*2);
    ptr = buf;

    for(int i=0; i<t_len; i++) {
      *ptr++ = template.buf_[i];
      *ptr++ = ':';
      size_t num_len = to_string(template.table_[template.buf_[i]], ptr);
      ptr += num_len;
      *ptr++ = ' ';
    }

    out_str_new_line(buf);
  }
}

void create_template() {
  new_line();
  memset(template.table_, 0, ASCCI_SIZE*sizeof(int));
  memset(template.buf_, 0, LINE_MAX_SIZE);
  
  strncpy(template.buf_, parser.buf_[1], LINE_MAX_SIZE);
  out_str_new_line(template.buf_);

  size_t s_len = strlen(template.buf_);
  template.sz_ = s_len;

  if(template.type_ == BM_ALGO) {
    size_t counter = 1;
    for(int i=s_len-2; i>=0; i--) {
      if(template.table_[template.buf_[i]] == 0)
        template.table_[template.buf_[i]] = counter;
      counter++;
    }

    template.table_[template.buf_[s_len-1]] = template.table_[template.buf_[s_len-1]]
      ? template.table_[template.buf_[s_len-1]]
      : MAX(counter-1, 1);
  }

  template.flag_loaded_ = 1;

  print_template();
}

void print_found_at_pos(size_t pos) {
  new_line();
  char buf[LINE_MAX_SIZE] = "Found '";
  char* ptr = buf+7;

  strncpy(ptr, template.buf_, template.sz_);
  ptr += template.sz_;
  *ptr++ = '\'';
  
  strncpy(ptr, " at pos: ", 9);
  ptr += 9;
  size_t num_len = to_string(pos, ptr);
  ptr += num_len;
  
  out_str_new_line(buf);
}

void print_not_found() {
  new_line();
  char buf[LINE_MAX_SIZE] = "Not Found '";
  char* ptr = buf+11;

  strncpy(ptr, template.buf_, template.sz_);
  ptr += template.sz_;
  *ptr++ = '\'';

  out_str_new_line(buf);
}

void search() {
  if(!template.flag_loaded_) {
    out_str_new_line("Template was not been loaded...");
    return;
  }

  size_t s_len = strlen(parser.buf_[1]);
  if(template.sz_ > s_len) {
    out_str_new_line("Template size too big...");
    return;
  }

  char* str = parser.buf_[1];

  if(template.type_ == STD_ALGO) {
    for(int i=0; i<s_len; i++) {
      size_t counter = 0;
      for(int j=0; j<template.sz_ && i+j<s_len; j++) {
        if(template.buf_[j] != str[i+j]) break;
        counter++;
      }

      if(counter == template.sz_) {
        print_found_at_pos(i);
        return;
      }
    }

    print_not_found();
  } else {
    size_t curr = template.sz_-1;
    int i = curr;
    size_t k = 0;
    while(i < s_len) {

      int j = curr;
      k = 0;
      for(; j>0; j--) {
        if(str[i-k] != template.buf_[j]) {
          if(j == curr) 
            i += (template.table_[str[i]] != 0) ? template.table_[str[i]] : template.sz_;
          else
            i += template.table_[template.buf_[j]];
          
          break;
        }

        k++;
      }

      if(j == 0) {
        print_found_at_pos(i-k);
        return;
      }
    }

    print_not_found();
  }
}

void test_upcase_1();
void test_downcase_1();
void test_titlize_1();

void tests_search_1();
void tests_search_2();
void tests_search_3();
void tests_search_4();

void tests_base() {
  test_upcase_1();
  test_downcase_1();
  test_titlize_1();
}

void tests_search() {
  tests_search_1();
  tests_search_2();
  tests_search_3();
  tests_search_4();
}

void shutdown() {
  outw (0x604, 0x2000);
}

void clear_all() {
  clear_screen();
  state.pos_x_ = 0; state.pos_y_ = 0;
  cursor_moveto(0, 0);
}

void handle_line() {  
  parse_curr_line();

  if(state.pos_y_ >= WINDOW_ROWS-1) {
    clear_all();
  }

  if(strncmp(parser.buf_[0], "info", 4) == 0) {
    info();
  } else if(strncmp(parser.buf_[0], "upcase", 6) == 0) {
    upcase();
  } else if(strncmp(parser.buf_[0], "downcase", 8) == 0) {
    downcase();
  } else if(strncmp(parser.buf_[0], "titlize", 7) == 0) {
    titlize();
  } else if(strncmp(parser.buf_[0], "template", 8) == 0) {
    create_template();
  } else if(strncmp(parser.buf_[0], "search", 7) == 0) {
    search();
  } else if(strncmp(parser.buf_[0], "shutdown", 8) == 0) {
    shutdown();
  } else if(strncmp(parser.buf_[0], "clear", 5) == 0) {
    clear_all();
  } else if(strncmp(parser.buf_[0], "tests", 5) == 0) {
    if(strncmp(parser.buf_[1], "search", 6) == 0) {
      tests_search();
    } else if(strncmp(parser.buf_[1], "base", 4) == 0){

      tests_base();
    }
  } else {
    new_line();
  }
}

/* ----------- Main ----------- */

#ifdef __cplusplus
extern "C"  {
#endif
int kmain() {
  uint8_t* start_value = (uint8_t*)0x100;
  state.start_value_ = *start_value;
  template.type_ = *start_value;
  
  intr_disable();
  intr_init();
  intr_start();
  keyb_init();
  intr_enable();

  // Init hashes
  init_upper();
  init_lower();
  init_codes_table();

  info();

  while(1) asm("hlt");

  intr_disable();

  return 0;
}
#ifdef __cplusplus
}
#endif


void test_upcase_1() {
  out_str_new_line("Test Upcase 1: [Input]   -> AbCaByuYuOpQ");
  out_str_new_line("Test Upcase 1: [Exp Out] -> ABCABYUYUOPQ");

  clear_parser();
  strncpy(parser.buf_[1], "AbCaByuYuOpQ", 12);
  parser.sz_ = 2;

  upcase();
  out_str_new_line("----------------");
}

void test_downcase_1() {
  out_str_new_line("Test Downcase 1: [Input]   -> AbCaByu-=:!ewqEWQ");
  out_str_new_line("Test Downcase 1: [Exp Out] -> abcabyu-=:!ewqewq");

  clear_parser();
  strncpy(parser.buf_[1], "AbCaByu-=:!ewqEWQ", 17);
  parser.sz_ = 2;

  downcase();
  out_str_new_line("----------------");
}

void test_titlize_1() {
  out_str_new_line("Test Titlize 1: [Input]   -> abC [byu-= e:!ew EqEWQ");
  out_str_new_line("Test Titlize 1: [Exp Out] -> AbC [byu-= E:!ew Eqewq");
  
  clear_parser();
  strncpy(parser.buf_[1], "abC", 3);
  strncpy(parser.buf_[2], "[byu-=", 6);
  strncpy(parser.buf_[3], "e:!ew", 5);
  strncpy(parser.buf_[4], "Eqewq", 5);
  parser.sz_ = 5;

  titlize();
  out_str_new_line("----------------");
}

void tests_search_1() {
  out_str_new_line("Template: 'ab' | algorithm: std");
  out_str_new_line("Test Search 1: [Input]   -> aaaaaabaab");
  out_str_new_line("Test Search 1: [Exp Out] -> pos: 5");
  
  clear_parser();
  size_t old = template.type_;

  template.flag_loaded_ = 1;
  template.type_ = STD_ALGO;  
  strncpy(parser.buf_[1], "aaaaaabaab", 10);
  strncpy(template.buf_, "ab", 2);
  template.sz_ = 2;
  parser.sz_ = 10;

  search();
  out_str_new_line("----------------");
  
  template.type_ = old;
}

void tests_search_2() {
  out_str_new_line("Template: 'ab' | algorithm: bm");
  out_str_new_line("Test Search 2: [Input]   -> aaaaaabaab");
  out_str_new_line("Test Search 2: [Exp Out] -> pos: 5");
  
  clear_parser();
  size_t old = template.type_;
  
  template.flag_loaded_ = 1;
  template.type_ = STD_ALGO;  
  strncpy(template.buf_, "ab", 2);
  strncpy(parser.buf_[1], "aaaaaabaab", 10);
  template.sz_ = 2;
  parser.sz_ = 10;

  search();
  out_str_new_line("----------------");

  template.type_ = old;
}

void tests_search_3() {
  out_str_new_line("Template: 'aba' | algorithm: std");
  out_str_new_line("Test Search 3: [Input]   -> aaaaaab");
  out_str_new_line("Test Search 3: [Exp Out] -> Not found");
  
  clear_parser();
  size_t old = template.type_;


  template.flag_loaded_ = 1;
  template.type_ = STD_ALGO;  
  strncpy(template.buf_, "aba", 3);
  strncpy(parser.buf_[1], "aaaaaab", 7);
  template.sz_ = 3;
  parser.sz_ = 7;

  search();
  out_str_new_line("----------------");
  
  template.type_ = old;
}

void tests_search_4() {
  out_str_new_line("Template: 'aba' | algorithm: bm");
  out_str_new_line("Test Search 4: [Input]   -> aaaaaab");
  out_str_new_line("Test Search 4: [Exp Out] -> Not found");
  
  clear_parser();
  size_t old = template.type_;

  template.flag_loaded_ = 1;
  template.type_ = BM_ALGO;  
  strncpy(template.buf_, "aba", 3);
  strncpy(parser.buf_[1], "aaaaaab", 7);
  template.sz_ = 3;
  parser.sz_ = 7;

  search();
  out_str_new_line("----------------");

  template.type_ = old;
}
