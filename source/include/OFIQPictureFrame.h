#pragma once

#include <wx/wx.h>
#include <wx/sizer.h>


// A scrolled window for showing an image.
class OFIQPictureFrame : public wxScrolledWindow
{
public:
    OFIQPictureFrame()
        : wxScrolledWindow()
        , m_bitmap(1, 1)
    {
        ;
    }

    void Create(wxWindow* parent, wxWindowID id = -1)
    {
        wxScrolledWindow::Create(parent, id);
    }

    void LoadImage(wxImage& image, double scale = 1.0 ) {
        int scaled_width = static_cast<int>(image.GetWidth() * scale);
        int scaled_height = static_cast<int>(image.GetHeight() * scale);
        if (scaled_width > 0 && scaled_height > 0)
        {
            m_bitmap = wxBitmap(image.Scale(scaled_width, scaled_height, wxIMAGE_QUALITY_HIGH));
            SetVirtualSize(scaled_width, scaled_height);
            wxClientDC dc(this);
            PrepareDC(dc);
            dc.DrawBitmap(m_bitmap, 0, 0);
            SetScrollbars(1, 1, scaled_width, scaled_height, 0, 0);
        }
        else
        {
            m_bitmap = wxBitmap(1,1);
            SetVirtualSize(1, 1);
            wxClientDC dc(this);
            PrepareDC(dc);
            dc.DrawBitmap(m_bitmap, 0, 0);
            SetScrollbars(1, 1, 1, 1, 0, 0);
        }
    }

protected:
    wxBitmap m_bitmap;

public:
    void OnMouse(wxMouseEvent& event) {
        int xx, yy;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &xx, &yy);
        event.m_x = xx; 
        event.m_y = yy;
        event.ResumePropagation(1); // Pass along mouse events (e.g. to parent)
        event.Skip();
    }

protected:
    void OnPaint(wxPaintEvent& event) {
        wxPaintDC dc(this);
        PrepareDC(dc);
        dc.DrawBitmap(m_bitmap, 0, 0, true);
    }
private:
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(OFIQPictureFrame, wxScrolledWindow)
EVT_PAINT(OFIQPictureFrame::OnPaint)
EVT_MOUSE_EVENTS(OFIQPictureFrame::OnMouse)
END_EVENT_TABLE()

class OFIQImagePanel : public wxScrolledWindow
{
    wxImage m_image;
    wxBitmap m_resized;
    int m_width, m_height;
    bool m_loaded;

    void (*m_onLeftClickBinding)(void*, int, int);

public:
    OFIQImagePanel(wxFrame* parent);

    bool FromImage(const wxImage& img);
    void Unload();
    void BindLeftClick(void (*onLeftClickBinding)(void*, int, int));

    double GetScale();
    wxSize GetImageSize();

    void paintEvent(wxPaintEvent& evt);
    void paintNow();
    void OnSize(wxSizeEvent& event);
    void clear(wxDC& dc);
    void render(wxDC& dc);


    //wxSize GetPreferredSize();

    // some useful events
    /*
     void mouseMoved(wxMouseEvent& event);
     void mouseDown(wxMouseEvent& event);
     void mouseWheelMoved(wxMouseEvent& event);
     void mouseReleased(wxMouseEvent& event);
     void rightClick(wxMouseEvent& event);
     void mouseLeftWindow(wxMouseEvent& event);
     void keyPressed(wxKeyEvent& event);
     void keyReleased(wxKeyEvent& event);
     */
    void OnLeftClick(wxMouseEvent& event);

    DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(OFIQImagePanel, wxPanel)
// some useful events
/*
 EVT_MOTION(wxImagePanel::mouseMoved)
 EVT_LEFT_DOWN(wxImagePanel::mouseDown)
 EVT_LEFT_UP(wxImagePanel::mouseReleased)
 EVT_RIGHT_DOWN(wxImagePanel::rightClick)
 EVT_LEAVE_WINDOW(wxImagePanel::mouseLeftWindow)
 EVT_KEY_DOWN(wxImagePanel::keyPressed)
 EVT_KEY_UP(wxImagePanel::keyReleased)
 EVT_MOUSEWHEEL(wxImagePanel::mouseWheelMoved)
 */

    EVT_LEFT_DOWN(OFIQImagePanel::OnLeftClick)

    // catch paint events
    EVT_PAINT(OFIQImagePanel::paintEvent)
    //Size event
    EVT_SIZE(OFIQImagePanel::OnSize)
    END_EVENT_TABLE()


    // some useful events
    /*
     void wxImagePanel::mouseMoved(wxMouseEvent& event) {}
     void wxImagePanel::mouseDown(wxMouseEvent& event) {}
     void wxImagePanel::mouseWheelMoved(wxMouseEvent& event) {}
     void wxImagePanel::mouseReleased(wxMouseEvent& event) {}
     void wxImagePanel::rightClick(wxMouseEvent& event) {}
     void wxImagePanel::mouseLeftWindow(wxMouseEvent& event) {}
     void wxImagePanel::keyPressed(wxKeyEvent& event) {}
     void wxImagePanel::keyReleased(wxKeyEvent& event) {}
     */

OFIQImagePanel::OFIQImagePanel(wxFrame* parent) :
    wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition)
{
    m_width = -1;
    m_height = -1;
    m_loaded = false;
    m_onLeftClickBinding = nullptr;
}

bool OFIQImagePanel::FromImage(const wxImage& img)
{
    wxClientDC dc(this);
    clear(dc);

    m_image = img;
    m_loaded = true;
    m_width = -1;
    m_height = -1;
    paintNow();
    SetScrollbars(1, 1, m_width, m_height, 0, 0);
    return m_loaded;
}

void OFIQImagePanel::Unload()
{
    wxClientDC dc(this);
    clear(dc);
    m_loaded = false;
    m_width = -1;
    m_height = -1;
    paintNow();
}

void OFIQImagePanel::BindLeftClick(void (*onLeftClickBinding)(void*, int, int))
{
    m_onLeftClickBinding = onLeftClickBinding;
}

double OFIQImagePanel::GetScale()
{
    double scale = -1.0;
    if (m_loaded)
    {
        auto size = GetSize();
        auto panel_width = size.GetWidth();
        auto panel_height = size.GetHeight();

        auto image_width = m_image.GetWidth();
        auto image_height = m_image.GetHeight();
        double horizontal_scale = (double)panel_width / (double)image_width;
        double vertical_scale = (double)panel_height / (double)image_height;

        scale = horizontal_scale < vertical_scale ? horizontal_scale : vertical_scale;
    }

    return scale;
}

wxSize OFIQImagePanel::GetImageSize()
{
    return m_resized.GetSize();
}

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

void OFIQImagePanel::paintEvent(wxPaintEvent& evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxPaintDC dc(this);
    render(dc);
}

/*
 * Alternatively, you can use a clientDC to paint on the panel
 * at any time. Using this generally does not free you from
 * catching paint events, since it is possible that e.g. the window
 * manager throws away your drawing when the window comes to the
 * background, and expects you will redraw it when the window comes
 * back (by sending a paint event).
 */
void OFIQImagePanel::paintNow()
{
    // depending on your system you may need to look at double-buffered dcs
    wxClientDC dc(this);
    render(dc);
}

void OFIQImagePanel::clear(wxDC& dc)
{
    dc.SetPen(*wxWHITE_PEN);
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.DrawRectangle(wxRect(0, 0, m_width, m_height));
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void OFIQImagePanel::render(wxDC& dc)
{
    try
    {
        if (m_loaded)
        {
            int neww, newh;
            dc.GetSize(&neww, &newh);

            if (neww != m_width || newh != m_height)
            {
                int image_width = m_image.GetWidth();
                int image_height = m_image.GetHeight();
                double horizontal_scale = (double)neww / (double)image_width;
                double vertical_scale = (double)newh / (double)image_height;

                double scale = horizontal_scale < vertical_scale ? horizontal_scale : vertical_scale;
                int scaled_width = (int)(scale * image_width);
                int scaled_height = (int)(scale * image_height);

                scaled_width = image_width;
                scaled_height = image_height;

                if (scaled_width > 0 && scaled_height > 0)
                {
                    m_resized = wxBitmap(m_image.Scale(scaled_width, scaled_height, wxIMAGE_QUALITY_HIGH));
                    m_width = scaled_width;
                    m_height = scaled_height;
                    dc.DrawBitmap(m_resized, 0, 0, false);
                }
                else
                {
                    m_width = -1;
                    m_height = -1;
                }
            }
            else {
                dc.DrawBitmap(m_resized, 0, 0, false);
            }
        }
    }
    catch (...)
    {
        std::cerr << "ERROR: Something went wrong during rendering." << std::endl;
    }
}

/*
 * Here we call refresh to tell the panel to draw itself again.
 * So when the user resizes the image panel the image should be resized too.
 */
void OFIQImagePanel::OnSize(wxSizeEvent& event)
{
    SetMinSize(wxSize(-1, -1));
    SetMaxSize(wxSize(-1, -1));
    Refresh();
    //skip the event.
    event.Skip();
}

void OFIQImagePanel::OnLeftClick(wxMouseEvent& event)
{
    if (m_onLeftClickBinding != nullptr)
    {
        (m_onLeftClickBinding)(GetParent(), event.GetX(), event.GetY());
    }
    return;
}