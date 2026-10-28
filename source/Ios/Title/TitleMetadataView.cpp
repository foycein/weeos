#include "Ios/Title/TitleMetadataView.h"

namespace Ios
{

namespace
{

ContentView MakeContentView(const Formats::ContentRecord& record)
{
    ContentView view;
    view.id = record.id;
    view.index = record.index;
    view.type = record.type;
    view.size = record.size;
    return view;
}

}

TitleMetadataView MakeTitleMetadataView(const Formats::TitleMetadata& metadata)
{
    TitleMetadataView view;
    view.version = metadata.version;
    view.requiredSystemVersion = TitleId::FromValue(metadata.requiredSystemVersion);
    view.titleId = TitleId::FromValue(metadata.titleId);
    view.titleType = metadata.titleType;
    view.groupId = metadata.groupId;
    view.region = metadata.region;
    view.ratings = metadata.ratings;
    view.inputOutputMask = metadata.inputOutputMask;
    view.titleVersion = metadata.titleVersion;

    for (const Formats::ContentRecord& record : metadata.contents)
        view.contents.push_back(MakeContentView(record));

    return view;
}

}
