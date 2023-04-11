// Copyright Epic Games, Inc. All Rights Reserved.

#include "remote_assetBPLibrary.h"
#include "remote_asset.h"
#include "IImageWrapperModule.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"


Uremote_assetBPLibrary::Uremote_assetBPLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

EImageFormat GetImageWrapperByUri(FString uri)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	if (uri.EndsWith(".png"))
	{
		return EImageFormat::PNG;
	}
	else if (uri.EndsWith(".jpg") || uri.EndsWith(".jpeg"))
	{
		return EImageFormat::JPEG;
	}
	else if (uri.EndsWith(".bmp"))
	{
		return EImageFormat::BMP;
	}
	else if (uri.EndsWith(".ico"))
	{
		return EImageFormat::ICO;

	}
	else if (uri.EndsWith("exr"))
	{
		return EImageFormat::EXR;
	}
	else if (uri.EndsWith(".icns"))
	{
		return EImageFormat::ICNS;
	}
	return EImageFormat::JPEG;
}


void Uremote_assetBPLibrary::LoadTexture2D(FString uri, const FLoadTexture2DCallBack callBack)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request = FHttpModule::Get().CreateRequest();
	request->SetVerb("GET");
	request->SetURL(uri);
	EImageFormat fileType = GetImageWrapperByUri(uri);
	request->OnProcessRequestComplete().BindLambda([callBack, fileType](FHttpRequestPtr HttpRequestPtr, FHttpResponsePtr HttpResponse, bool bSuccess)
		{
			UTexture2D* OutTex = TYPE_OF_NULLPTR();
			auto res = HttpResponse.Get();
			int code = res->GetResponseCode();
			if (code != 200) {
				callBack.ExecuteIfBound(OutTex, false);
			}
			IImageWrapperModule& imageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			auto imageWrapper = imageWrapperModule.CreateImageWrapper(fileType);
			TArray<uint8> OutArray;
			OutArray = HttpResponse.Get()->GetContent();
			if (imageWrapper.IsValid() &&
				imageWrapper->SetCompressed(OutArray.GetData(), OutArray.Num()))
			{
				TArray<uint8> uncompressedRGBA;
				if (imageWrapper->GetRaw(ERGBFormat::RGBA, 8, uncompressedRGBA))
				{
					TArray<FColor> uncompressedFColor = uint8ToFColor(uncompressedRGBA);
					OutTex = TextureFromImage(
						imageWrapper->GetWidth(),
						imageWrapper->GetHeight(),
						uncompressedFColor,
						true);
				}
			}
			callBack.ExecuteIfBound(OutTex, true);
		}); //请求回调
	request->ProcessRequest();
}


TArray<FColor> Uremote_assetBPLibrary::uint8ToFColor(TArray<uint8> origin)
{
	TArray<FColor> uncompressedFColor;
	uint8 auxOrigin;
	FColor auxDst;

	for (int i = 0; i < origin.Num(); i++) {
		auxOrigin = origin[i];
		auxDst.R = auxOrigin;
		i++;
		auxOrigin = origin[i];
		auxDst.G = auxOrigin;
		i++;
		auxOrigin = origin[i];
		auxDst.B = auxOrigin;
		i++;
		auxOrigin = origin[i];
		auxDst.A = auxOrigin;
		uncompressedFColor.Add(auxDst);
	}
	return  uncompressedFColor;
}

UTexture2D* Uremote_assetBPLibrary::TextureFromImage(const int32 SrcWidth, const int32 SrcHeight, const TArray<FColor>& SrcData, const bool UseAlpha)
{
	// 创建Texture2D纹理
	UTexture2D* MyScreenshot = UTexture2D::CreateTransient(SrcWidth, SrcHeight, PF_B8G8R8A8);
	// 锁住他的数据，以便修改
	uint8* MipData = static_cast<uint8*>(MyScreenshot->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	// 创建纹理数据
	uint8* DestPtr = NULL;
	const FColor* SrcPtr = NULL;
	for (int32 y = 0; y < SrcHeight; y++)
	{
		DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];
		SrcPtr = const_cast<FColor*>(&SrcData[(SrcHeight - 1 - y) * SrcWidth]);
		for (int32 x = 0; x < SrcWidth; x++)
		{
			*DestPtr++ = SrcPtr->B;
			*DestPtr++ = SrcPtr->G;
			*DestPtr++ = SrcPtr->R;
			if (UseAlpha)
			{
				*DestPtr++ = SrcPtr->A;
			}
			else
			{
				*DestPtr++ = 0xFF;
			}
			SrcPtr++;
		}
	}
	// 解锁纹理
	MyScreenshot->GetPlatformData()->Mips[0].BulkData.Unlock();
	MyScreenshot->UpdateResource();
	return MyScreenshot;
}
