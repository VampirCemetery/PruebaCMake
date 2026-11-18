// PruebaCMake.cpp - Editor Win32 Organizado + OpenCV
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <algorithm>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "comctl32.lib")

// Identificadores de Controles
#define ID_BTN_ABRIR       201
#define ID_BTN_RESTABLECER 202
#define ID_BTN_GUARDAR     203
#define ID_BTN_INVERTIR    204
#define ID_BTN_ESPEJO      205
#define ID_SLIDER_BLUR     206
#define ID_SLIDER_LAPIZ    207

// Variables Globales
HWND g_hwndMain = nullptr;
HWND g_hwndImageCanvas = nullptr;
HWND g_hwndSliderBlur = nullptr;
HWND g_hwndSliderLapiz = nullptr;
HWND g_hwndBtnInvertir = nullptr;
HWND g_hwndBtnEspejo = nullptr;
HWND g_hwndBtnAbrir = nullptr;
HWND g_hwndBtnRestablecer = nullptr;
HWND g_hwndBtnGuardar = nullptr;
HWND g_hwndLabelBlur = nullptr;
HWND g_hwndLabelLapiz = nullptr;

cv::Mat g_imagenOriginal;
cv::Mat g_imagenActual;

bool g_estadoInvertir = false;
bool g_estadoEspejo = false;

// Convierte WCHAR a std::string ANSI para OpenCV
static std::string RutaANSI(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_ACP, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 1 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_ACP, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

// Redimensiona proporcionalmente respetando la relación de aspecto
cv::Mat EscaladoProporcional(const cv::Mat& src, int maxAncho, int maxAlto) {
    if (src.empty() || maxAncho <= 0 || maxAlto <= 0) return src;

    double ratioAncho = (double)maxAncho / src.cols;
    double ratioAlto = (double)maxAlto / src.rows;
    double ratio = (std::min)(ratioAncho, ratioAlto);

    cv::Mat dst;
    cv::resize(src, dst, cv::Size(), ratio, ratio, cv::INTER_AREA);
    return dst;
}

// Dibuja la matriz de OpenCV en el control nativo
void DibujarMatrizEnControl(HWND hwndControl, const cv::Mat& imgBGR) {
    if (imgBGR.empty() || !hwndControl) return;

    RECT rc;
    GetClientRect(hwndControl, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    cv::Mat imgEscalada = EscaladoProporcional(imgBGR, winW, winH);

    cv::Mat imgBGRA;
    cv::cvtColor(imgEscalada, imgBGRA, cv::COLOR_BGR2BGRA);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = imgBGRA.cols;
    bmi.bmiHeader.biHeight = -imgBGRA.rows;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(hwndControl);
    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (hBitmap && pBits) {
        memcpy(pBits, imgBGRA.data, imgBGRA.total() * imgBGRA.elemSize());
        HBITMAP hOldBmp = (HBITMAP)SendMessage(hwndControl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
        if (hOldBmp) DeleteObject(hOldBmp);
    }
    ReleaseDC(hwndControl, hdc);
}

// Procesamiento de Filtros
void ProcesarFiltros() {
    if (g_imagenOriginal.empty()) return;

    cv::Mat resultado = g_imagenOriginal.clone();

    int valBlur = (int)SendMessage(g_hwndSliderBlur, TBM_GETPOS, 0, 0);
    int valLapiz = (int)SendMessage(g_hwndSliderLapiz, TBM_GETPOS, 0, 0);

    if (valBlur > 0) {
        int ksize = (valBlur * 2) + 1;
        cv::GaussianBlur(resultado, resultado, cv::Size(ksize, ksize), 0);
    }

    if (valLapiz > 0) {
        cv::Mat imgGris, imgColor;
        cv::pencilSketch(resultado, imgGris, imgColor, (float)valLapiz, 0.07f, 0.02f);
        cv::cvtColor(imgGris, resultado, cv::COLOR_GRAY2BGR);
    }

    if (g_estadoInvertir) {
        cv::bitwise_not(resultado, resultado);
    }

    if (g_estadoEspejo) {
        cv::flip(resultado, resultado, 1);
    }

    g_imagenActual = resultado;
    DibujarMatrizEnControl(g_hwndImageCanvas, g_imagenActual);
}

// Reorganiza posiciones de controles al redimensionar/maximizar la ventana
void AcomodarElementos(int anchoVentana, int altoVentana) {
    if (anchoVentana <= 0 || altoVentana <= 0) return;

    int margen = 20;
    int altoBotonesTop = 32;
    int altoAreaControlesBot = 130; // Espacio reservado abajo para las barras y botones

    // 1. Botones Superiores (Abrir, Restablecer, Guardar)
    MoveWindow(g_hwndBtnAbrir, margen, 15, 140, altoBotonesTop, TRUE);
    MoveWindow(g_hwndBtnRestablecer, margen + 150, 15, 140, altoBotonesTop, TRUE);
    MoveWindow(g_hwndBtnGuardar, margen + 300, 15, 160, altoBotonesTop, TRUE);

    // 2. Imagen Central (Ocupa desde y=55 hasta antes del panel inferior)
    int yImagen = 55;
    int anchoImagen = anchoVentana - (margen * 2);
    int altoImagen = altoVentana - yImagen - altoAreaControlesBot - 10;
    if (altoImagen < 100) altoImagen = 100;

    MoveWindow(g_hwndImageCanvas, margen, yImagen, anchoImagen, altoImagen, TRUE);

    // 3. Panel Inferior (Debajo de la imagen)
    int yInferior = yImagen + altoImagen + 10;

    // Columna Izquierda: Barras en vertical (Blur arriba, Lápiz abajo)
    int anchoSliders = 380;
    MoveWindow(g_hwndLabelBlur, margen, yInferior, 120, 18, TRUE);
    MoveWindow(g_hwndSliderBlur, margen, yInferior + 18, anchoSliders, 30, TRUE);

    MoveWindow(g_hwndLabelLapiz, margen, yInferior + 52, 120, 18, TRUE);
    MoveWindow(g_hwndSliderLapiz, margen, yInferior + 70, anchoSliders, 30, TRUE);

    // Columna Derecha: Botones en vertical (Invertir arriba, Espejo abajo)
    int xBotonesDer = margen + anchoSliders + 40;
    MoveWindow(g_hwndBtnInvertir, xBotonesDer, yInferior + 10, 150, 35, TRUE);
    MoveWindow(g_hwndBtnEspejo, xBotonesDer, yInferior + 55, 150, 35, TRUE);

    if (!g_imagenActual.empty()) {
        DibujarMatrizEnControl(g_hwndImageCanvas, g_imagenActual);
    }
}

void RestablecerFiltros() {
    g_estadoInvertir = false;
    g_estadoEspejo = false;

    SendMessage(g_hwndBtnInvertir, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(g_hwndBtnEspejo, BM_SETCHECK, BST_UNCHECKED, 0);

    SendMessage(g_hwndSliderBlur, TBM_SETPOS, TRUE, 0);
    SendMessage(g_hwndSliderLapiz, TBM_SETPOS, TRUE, 0);

    if (!g_imagenOriginal.empty()) {
        ProcesarFiltros();
    }
}

void AbrirImagen() {
    wchar_t ruta[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"Imágenes\0*.png;*.jpg;*.jpeg;*.bmp\0Todos\0*.*\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        g_imagenOriginal = cv::imread(RutaANSI(ruta));
        if (g_imagenOriginal.empty()) {
            MessageBoxW(g_hwndMain, L"No se pudo cargar la imagen.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
        RestablecerFiltros();
    }
}

void GuardarImagen() {
    if (g_imagenActual.empty()) {
        MessageBoxW(g_hwndMain, L"No hay ninguna imagen cargada para guardar.", L"Advertencia", MB_OK | MB_ICONWARNING);
        return;
    }

    wchar_t ruta[MAX_PATH] = L"resultado.png";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"Imagen PNG (*.png)\0*.png\0Imagen JPG (*.jpg)\0*.jpg\0";
    ofn.lpstrFile = ruta;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        if (cv::imwrite(RutaANSI(ruta), g_imagenActual)) {
            MessageBoxW(g_hwndMain, L"¡Imagen procesada guardada con éxito!", L"Guardado", MB_OK | MB_ICONINFORMATION);
        }
        else {
            MessageBoxW(g_hwndMain, L"Error al guardar la imagen.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
        InitCommonControlsEx(&icex);

        // 1. Botones Arriba
        g_hwndBtnAbrir = CreateWindowExW(0, L"BUTTON", L"Abrir Imagen", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_BTN_ABRIR, nullptr, nullptr);
        g_hwndBtnRestablecer = CreateWindowExW(0, L"BUTTON", L"Restablecer", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_BTN_RESTABLECER, nullptr, nullptr);
        g_hwndBtnGuardar = CreateWindowExW(0, L"BUTTON", L"Guardar Resultado", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_BTN_GUARDAR, nullptr, nullptr);

        // 2. Canvas de Imagen (Centro)
        g_hwndImageCanvas = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);

        // 3. Controles Inferiores
        g_hwndLabelBlur = CreateWindowExW(0, L"STATIC", L"Nivel de Blur:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g_hwndSliderBlur = CreateWindowExW(0, TRACKBAR_CLASSW, L"BlurSlider", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, hwnd, (HMENU)ID_SLIDER_BLUR, nullptr, nullptr);
        SendMessage(g_hwndSliderBlur, TBM_SETRANGE, TRUE, MAKELONG(0, 20));

        g_hwndLabelLapiz = CreateWindowExW(0, L"STATIC", L"Efecto Lápiz:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
        g_hwndSliderLapiz = CreateWindowExW(0, TRACKBAR_CLASSW, L"LapizSlider", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, hwnd, (HMENU)ID_SLIDER_LAPIZ, nullptr, nullptr);
        SendMessage(g_hwndSliderLapiz, TBM_SETRANGE, TRUE, MAKELONG(0, 100));

        g_hwndBtnInvertir = CreateWindowExW(0, L"BUTTON", L"Invertir Colores", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, 0, 0, 0, 0, hwnd, (HMENU)ID_BTN_INVERTIR, nullptr, nullptr);
        g_hwndBtnEspejo = CreateWindowExW(0, L"BUTTON", L"Efecto Espejo", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, 0, 0, 0, 0, hwnd, (HMENU)ID_BTN_ESPEJO, nullptr, nullptr);

        return 0;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        AcomodarElementos(width, height);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_BTN_ABRIR) AbrirImagen();
        else if (id == ID_BTN_RESTABLECER) RestablecerFiltros();
        else if (id == ID_BTN_INVERTIR) {
            g_estadoInvertir = !g_estadoInvertir;
            ProcesarFiltros();
        }
        else if (id == ID_BTN_ESPEJO) {
            g_estadoEspejo = !g_estadoEspejo;
            ProcesarFiltros();
        }
        else if (id == ID_BTN_GUARDAR) GuardarImagen();
        return 0;
    }

    case WM_HSCROLL: {
        ProcesarFiltros();
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"VentanaEditorWin32OpenCV";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    // Habilitar ventana REDIMENSIONABLE y MAXIMIZABLE (WS_OVERLAPPEDWINDOW)
    g_hwndMain = CreateWindowExW(0, wc.lpszClassName, L"Editor de Imágenes - OpenCV",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 750,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(g_hwndMain, nShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}