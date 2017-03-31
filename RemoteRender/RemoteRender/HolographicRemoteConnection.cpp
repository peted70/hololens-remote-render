#include "pch.h"
#include "HolographicRemoteConnection.h"
#include "DebugLog.h"
#include <functional>

HolographicRemoteConnection::HolographicRemoteConnection(const shared_ptr<DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources)
{
}

void HolographicRemoteConnection::Connect(String^ ipAddress,
	function<void()> OnConnected,
	function<void(HolographicStreamerConnectionFailureReason)> OnDisconnected,
	function<void(const ComPtr<ID3D11Texture2D>&)> OnPreviewFrame)
{
	m_streamerHelpers = ref new HolographicStreamerHelpers();
	m_streamerHelpers->CreateStreamer(m_deviceResources->GetD3DDevice());

	// We currently need to stream at 720p because that's the resolution of our remote display.
	// There is a check in the holographic streamer that makes sure the remote and local
	// resolutions match. The default streamer resolution is 1080p.
	m_streamerHelpers->SetVideoFrameSize(1280, 720);

	ConnectHandlers(ipAddress, m_streamerHelpers, OnConnected, OnDisconnected, OnPreviewFrame);

	try
	{
		m_streamerHelpers->Connect(ipAddress->Data(), 8001);
	}
	catch (Exception^ ex)
	{
		DebugLog(L"Connect failed with hr = 0x%08X", ex->HResult);
	}
}

void HolographicRemoteConnection::ConnectHandlers(String^ ipAddress,
	HolographicStreamerHelpers^ helper,
	function<void()> OnConnected, function<void(HolographicStreamerConnectionFailureReason)> OnDisconnected, 
	function<void(const ComPtr<ID3D11Texture2D>&)> OnPreviewFrame)
{
	helper->OnConnected += ref new ConnectedEvent(OnConnected);

	Platform::WeakReference streamerHelpersWeakRef = Platform::WeakReference(m_streamerHelpers);
	helper->OnDisconnected += ref new DisconnectedEvent(
		[this, streamerHelpersWeakRef, OnDisconnected, ipAddress]
			(_In_ HolographicStreamerConnectionFailureReason failureReason)
	{
		DebugLog(L"Disconnected with reason %d", failureReason);

		OnDisconnected(failureReason);

		// Reconnect if this is a transient failure.
		if (failureReason == HolographicStreamerConnectionFailureReason::Unreachable ||
			failureReason == HolographicStreamerConnectionFailureReason::ConnectionLost)
		{
			DebugLog(L"Reconnecting...");

			try
			{
				auto helpersResolved = streamerHelpersWeakRef.Resolve<HolographicStreamerHelpers>();
				if (helpersResolved)
				{
					helpersResolved->Connect(ipAddress->Data(), 8001);
				}
				else
				{
					DebugLog(L"Failed to reconnect because a disconnect has already occurred.\n");
				}
			}
			catch (Platform::Exception^ ex)
			{
				DebugLog(L"Connect failed with hr = 0x%08X", ex->HResult);
			}
		}
		else
		{
			DebugLog(L"Disconnected with unrecoverable error, not attempting to reconnect.");
		}
	});

	helper->OnSendFrame += ref new SendFrameEvent(
		[this, OnPreviewFrame](_In_ const ComPtr<ID3D11Texture2D>& spTexture, _In_ FrameMetadata metadata)
	{
		OnPreviewFrame(spTexture);
	});
};