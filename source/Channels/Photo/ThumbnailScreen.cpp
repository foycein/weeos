#include "Channels/Photo/PhotoPictures.h"
#include "Channels/Photo/PhotoSession.h"
#include "Channels/Photo/ShownPicture.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>

// The list of the photos and movies of the card (layout "thumb"): rows of small pictures between the two rows of buttons, scrolled
// with Up and Down, made larger and smaller with the zoom buttons (+ and -), opened with A.

namespace Channels
{

namespace
{

// The place of the list and how many columns each zoom shows. The original draws the list itself; these numbers are Assumed (CHN-29).
constexpr float ListTop = 150;
constexpr float ListBottom = -150;
constexpr float ListSideMargin = 40;
constexpr float CellAspect = 3.0f / 4.0f;
constexpr float CellGap = 6;
constexpr std::array<int, 3> ColumnsOfZoom = {3, 5, 8};
constexpr int FirstZoom = 1;
constexpr int ThumbnailsMadePerStep = 2;
constexpr Formats::Byte UnsupportedGray = 90;

// A file whose format the channel does not support shows as a gray field in the list.
std::unique_ptr<ShownPicture> MakeUnsupportedMarker(Graphics::GraphicsDevice& device)
{
    constexpr Formats::Byte Opaque = 255;
    Formats::RgbaImage gray{1, 1, {{UnsupportedGray, UnsupportedGray, UnsupportedGray, Opaque}}};
    return std::make_unique<ShownPicture>(device, gray);
}

class ThumbnailScreen : public PhotoScreen
{
public:
    ThumbnailScreen(PhotoSession& photoSession, int firstShownFile)
        : session(photoSession)
        , buttons(session.MakeLayout("thumb"))
        , quitQuestion(session.MakeLayout("thumb_quit1"))
        , unsupportedMarker(MakeUnsupportedMarker(session.Services().display.Device()))
    {
        firstRow = firstShownFile / Columns();
    }

    void Update() override
    {
        buttons->Calculate();
        quitQuestion->Calculate();
        MakeSomeThumbnails();

        if (isAskingToQuit)
            UpdateQuitQuestion();
        else
            UpdateList();
    }

    void Draw() override
    {
        for (int index = FirstShownFile(); index < EndOfShownFiles(); ++index)
            DrawThumbnail(index);

        buttons->Draw();
        if (isAskingToQuit)
            quitQuestion->Draw();
    }

    std::string_view Name() const override
    {
        return isAskingToQuit ? "Return to the menu?" : "Thumbnails";
    }

private:
    int Columns() const
    {
        return ColumnsOfZoom[zoom];
    }

    float CellWidth() const
    {
        float listWidth = session.Services().display.GetCanvas().width - 2 * ListSideMargin;
        return listWidth / Columns();
    }

    float CellHeight() const
    {
        return CellWidth() * CellAspect;
    }

    int VisibleRows() const
    {
        return std::max(1, static_cast<int>((ListTop - ListBottom) / CellHeight()));
    }

    int RowCount() const
    {
        int fileCount = static_cast<int>(session.library.files.size());
        return (fileCount + Columns() - 1) / Columns();
    }

    int FirstShownFile() const
    {
        return firstRow * Columns();
    }

    int EndOfShownFiles() const
    {
        int end = (firstRow + VisibleRows()) * Columns();
        return std::min(end, static_cast<int>(session.library.files.size()));
    }

    PictureBox BoxOfFile(int index) const
    {
        int row = index / Columns() - firstRow;
        int column = index % Columns();
        float left = -session.Services().display.GetCanvas().width / 2 + ListSideMargin;

        PictureBox box;
        box.centerX = left + (column + 0.5f) * CellWidth();
        box.centerY = ListTop - (row + 0.5f) * CellHeight();
        box.width = CellWidth() - CellGap;
        box.height = CellHeight() - CellGap;
        return box;
    }

    void MakeSomeThumbnails()
    {
        int made = 0;
        for (int index = FirstShownFile(); index < EndOfShownFiles() && made < ThumbnailsMadePerStep; ++index)
        {
            if (thumbnails.contains(index) || unsupportedFiles.contains(index))
                continue;

            MakeThumbnail(index);
            ++made;
        }
    }

    void MakeThumbnail(int index)
    {
        const PhotoFile& file = session.library.files[index];
        std::optional<Formats::Bytes> bytes = ReadCardFile(session.Services().storage.sdCard, file.path);
        std::optional<Formats::RgbaImage> picture;
        if (bytes)
            picture = Channels::MakeThumbnail(*bytes, file.kind);

        if (!picture)
        {
            unsupportedFiles.insert(index);
            return;
        }
        thumbnails[index] = std::make_unique<ShownPicture>(session.Services().display.Device(), *picture);
    }

    void DrawThumbnail(int index)
    {
        PictureBox box = BoxOfFile(index);
        auto thumbnail = thumbnails.find(index);
        if (thumbnail != thumbnails.end())
        {
            thumbnail->second->DrawInside(box);
            return;
        }

        if (unsupportedFiles.contains(index))
            unsupportedMarker->DrawInside(box);
    }

    void UpdateList()
    {
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*buttons, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            isAskingToQuit = true;
        }
        else if (session.IsButtonPressed(*buttons, "slide"))
        {
            services.sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeSlideShowScreen(session, FirstShownFile()));
        }
        else if (session.IsButtonPressed(*buttons, "up") || session.IsRemoteButtonPressed(Input::RemoteButtons::Up))
        {
            Scroll(-1);
        }
        else if (session.IsButtonPressed(*buttons, "down") || session.IsRemoteButtonPressed(Input::RemoteButtons::Down))
        {
            Scroll(1);
        }
        else if (session.IsButtonPressed(*buttons, "zoom_in") || session.IsRemoteButtonPressed(Input::RemoteButtons::Plus))
        {
            ChangeZoom(-1, PhotoSound::ZoomIn);
        }
        else if (session.IsButtonPressed(*buttons, "zoom_out") || session.IsRemoteButtonPressed(Input::RemoteButtons::Minus))
        {
            ChangeZoom(1, PhotoSound::ZoomOut);
        }
        else
        {
            OpenPressedFile();
        }
    }

    void Scroll(int rows)
    {
        int lastFirstRow = std::max(0, RowCount() - VisibleRows());
        int newFirstRow = std::clamp(firstRow + rows, 0, lastFirstRow);
        if (newFirstRow == firstRow)
            return;

        firstRow = newFirstRow;
        session.Services().sounds.Request(PhotoSound::Scroll);
    }

    void ChangeZoom(int step, std::string_view sound)
    {
        int newZoom = std::clamp(zoom + step, 0, static_cast<int>(ColumnsOfZoom.size()) - 1);
        if (newZoom == zoom)
            return;

        int firstFile = FirstShownFile();
        zoom = newZoom;
        firstRow = firstFile / Columns();
        session.Services().sounds.Request(sound);
    }

    void OpenPressedFile()
    {
        std::optional<Formats::Vector2> place = session.PlaceOfPress();
        if (!place)
            return;

        for (int index = FirstShownFile(); index < EndOfShownFiles(); ++index)
        {
            PictureBox box = BoxOfFile(index);
            bool isInside = std::abs(place->x - box.centerX) <= box.width / 2 && std::abs(place->y - box.centerY) <= box.height / 2;
            if (!isInside)
                continue;

            session.Services().sounds.Request(PhotoSound::SelectPhoto);
            session.GoTo(MakeViewScreen(session, index));
            return;
        }
    }

    void UpdateQuitQuestion()
    {
        if (session.IsButtonPressed(*quitQuestion, "yes"))
        {
            session.Services().sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeTitleScreen(session));
        }
        else if (session.IsButtonPressed(*quitQuestion, "no"))
        {
            session.Services().sounds.Request(PhotoSound::Cancel);
            isAskingToQuit = false;
        }
    }

    PhotoSession& session;
    std::unique_ptr<SystemMenu::MenuLayout> buttons;
    std::unique_ptr<SystemMenu::MenuLayout> quitQuestion;
    std::map<int, std::unique_ptr<ShownPicture>> thumbnails;
    std::set<int> unsupportedFiles;
    std::unique_ptr<ShownPicture> unsupportedMarker;
    int zoom = FirstZoom;
    int firstRow = 0;
    bool isAskingToQuit = false;
};

}

std::unique_ptr<PhotoScreen> MakeThumbnailScreen(PhotoSession& session, int firstShownFile)
{
    return std::make_unique<ThumbnailScreen>(session, firstShownFile);
}

}
